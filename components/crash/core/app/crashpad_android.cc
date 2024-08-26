// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/crash/core/app/crashpad.h"

#include <dlfcn.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <string_view>

#include "base/android/build_info.h"
#include "base/android/java_exception_reporter.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "content/public/common/content_descriptors.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/tagging.h"
#include "sandbox/linux/services/syscall_wrappers.h"
#include "third_party/crashpad/crashpad/client/annotation.h"
#include "third_party/crashpad/crashpad/client/client_argv_handling.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/simulate_crash_linux.h"
#include "third_party/crashpad/crashpad/snapshot/sanitized/sanitization_information.h"
#include "third_party/crashpad/crashpad/util/linux/exception_handler_client.h"
#include "third_party/crashpad/crashpad/util/linux/exception_handler_protocol.h"
#include "third_party/crashpad/crashpad/util/linux/exception_information.h"
#include "third_party/crashpad/crashpad/util/linux/scoped_pr_set_dumpable.h"
#include "third_party/crashpad/crashpad/util/misc/from_pointer_cast.h"
#include "third_party/crashpad/crashpad/util/posix/signals.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/crash/android/package_paths_jni/PackagePaths_jni.h"

namespace crashpad {
namespace {

class AllowedMemoryRanges {
 public:
  AllowedMemoryRanges() {
    allowed_memory_ranges_.entries = 0;
    allowed_memory_ranges_.size = 0;
  }

  AllowedMemoryRanges(const AllowedMemoryRanges&) = delete;
  AllowedMemoryRanges& operator=(const AllowedMemoryRanges&) = delete;

  void AddEntry(VMAddress base, VMSize length) {
    SanitizationAllowedMemoryRanges::Range new_entry;
    new_entry.base = base;
    new_entry.length = length;

    base::AutoLock lock(lock_);
    std::vector<SanitizationAllowedMemoryRanges::Range> new_array(array_);
    new_array.push_back(new_entry);
    allowed_memory_ranges_.entries =
        FromPointerCast<VMAddress>(new_array.data());
    allowed_memory_ranges_.size += 1;
    array_ = std::move(new_array);
  }

  SanitizationAllowedMemoryRanges* GetSanitizationAddress() {
    return &allowed_memory_ranges_;
  }

  static AllowedMemoryRanges* Singleton() {
    static base::NoDestructor<AllowedMemoryRanges> singleton;
    return singleton.get();
  }

 private:
  base::Lock lock_;
  SanitizationAllowedMemoryRanges allowed_memory_ranges_;
  std::vector<SanitizationAllowedMemoryRanges::Range> array_;
};

bool SetSanitizationInfo(crash_reporter::CrashReporterClient* client,
                         SanitizationInformation* info) {
  const char* const* allowed_annotations = nullptr;
  void* target_module = nullptr;
  bool sanitize_stacks = false;
  client->GetSanitizationInformation(&allowed_annotations, &target_module,
                                     &sanitize_stacks);
  info->allowed_annotations_address =
      FromPointerCast<VMAddress>(allowed_annotations);
  info->target_module_address = FromPointerCast<VMAddress>(target_module);
  info->allowed_memory_ranges_address = FromPointerCast<VMAddress>(
      AllowedMemoryRanges::Singleton()->GetSanitizationAddress());
  info->sanitize_stacks = sanitize_stacks;
  return allowed_annotations != nullptr || target_module != nullptr ||
         sanitize_stacks;
}

void SetExceptionInformation(siginfo_t* siginfo,
                             ucontext_t* context,
                             ExceptionInformation* info) {
  info->siginfo_address =
      FromPointerCast<decltype(info->siginfo_address)>(siginfo);
  info->context_address =
      FromPointerCast<decltype(info->context_address)>(context);
  info->thread_id = sandbox::sys_gettid();
}

void SetClientInformation(ExceptionInformation* exception,
                          SanitizationInformation* sanitization,
                          ExceptionHandlerProtocol::ClientInformation* info) {
  info->exception_information_address =
      FromPointerCast<decltype(info->exception_information_address)>(exception);

  info->sanitization_information_address =
      FromPointerCast<decltype(info->sanitization_information_address)>(
          sanitization);
}

// A signal handler for non-browser processes in the sandbox.
// Sends a message to a crashpad::CrashHandlerHost to handle the crash.
class SandboxedHandler {
 public:
  static SandboxedHandler* Get() {
    static SandboxedHandler* instance = new SandboxedHandler();
    return instance;
  }

  SandboxedHandler(const SandboxedHandler&) = delete;
  SandboxedHandler& operator=(const SandboxedHandler&) = delete;

  bool Initialize(bool dump_at_crash) {
    request_dump_ = dump_at_crash ? 1 : 0;

    SetSanitizationInfo(crash_reporter::GetCrashReporterClient(),
                        &sanitization_);
    server_fd_ = base::GlobalDescriptors::GetInstance()->Get(kCrashDumpSignal);

    // Android's debuggerd handler on JB MR2 until OREO displays a dialog which
    // is a bad user experience for child process crashes. Disable the debuggerd
    // handler for user builds. crbug.com/273706
    base::android::BuildInfo* build_info =
        base::android::BuildInfo::GetInstance();
    restore_previous_handler_ =
        build_info->sdk_int() < base::android::SDK_VERSION_JELLY_BEAN_MR2 ||
        build_info->sdk_int() >= base::android::SDK_VERSION_OREO ||
        strcmp(build_info->build_type(), "eng") == 0 ||
        strcmp(build_info->build_type(), "userdebug") == 0;

    bool signal_stack_initialized =
        CrashpadClient::InitializeSignalStackForThread();
    DCHECK(signal_stack_initialized);
    return Signals::InstallCrashHandlers(HandleCrash, SA_ONSTACK,
                                         &old_actions_);
  }

  void HandleCrashNonFatal(int signo, siginfo_t* siginfo, void* context) {
    base::ScopedFD connection;
    if (ConnectToHandler(signo, &connection) == 0) {
      ExceptionInformation exception_information;
      SetExceptionInformation(siginfo, static_cast<ucontext_t*>(context),
                              &exception_information);

      ExceptionHandlerProtocol::ClientInformation info;
      SetClientInformation(&exception_information, &sanitization_, &info);

      ScopedPrSetDumpable set_dumpable(/* may_log= */ false);

      ExceptionHandlerClient handler_client(connection.get(), false);
      handler_client.SetCanSetPtracer(false);
      handler_client.RequestCrashDump(info);
    }
  }

  using CrashHandlerFunc = bool (*)(int, siginfo_t*, ucontext_t*);
  void SetLastChanceExceptionHandler(CrashHandlerFunc handler) {
    last_chance_handler_ = handler;
  }

 private:
  SandboxedHandler() = default;
  ~SandboxedHandler() = delete;

  int ConnectToHandler(int signo, base::ScopedFD* connection) {
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
      return errno;
    }
    base::ScopedFD local_connection(fds[0]);
    base::ScopedFD handlers_socket(fds[1]);

    // SELinux may block the handler from setting SO_PASSCRED on this socket.
    // Attempt to set it here, but the handler can still try if this fails.
    int optval = 1;
    socklen_t optlen = sizeof(optval);
    setsockopt(handlers_socket.get(), SOL_SOCKET, SO_PASSCRED, &optval, optlen);

    iovec iov[2];
    iov[0].iov_base = &signo;
    iov[0].iov_len = sizeof(signo);
    iov[1].iov_base = &request_dump_;
    iov[1].iov_len = sizeof(request_dump_);

    msghdr msg;
    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = std::size(iov);

    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *reinterpret_cast<int*>(CMSG_DATA(cmsg)) = handlers_socket.get();

    if (HANDLE_EINTR(sendmsg(server_fd_, &msg, MSG_NOSIGNAL)) < 0) {
      return errno;
    }

    *connection = std::move(local_connection);
    return 0;
  }

  static void HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    SandboxedHandler* state = Get();
    state->HandleCrashNonFatal(signo, siginfo, context);
    if (state->last_chance_handler_ &&
        state->last_chance_handler_(signo, siginfo,
                                    static_cast<ucontext_t*>(context))) {
      return;
    }
    Signals::RestoreHandlerAndReraiseSignalOnReturn(
        siginfo, state->restore_previous_handler_
                     ? state->old_actions_.ActionForSignal(signo)
                     : nullptr);
  }

  Signals::OldActions old_actions_ = {};
  SanitizationInformation sanitization_;
  int server_fd_;
  unsigned char request_dump_;
  CrashHandlerFunc last_chance_handler_;

  // true if the previously installed signal handler is restored after
  // handling a crash. Otherwise SIG_DFL is restored.
  bool restore_previous_handler_;
};

}  // namespace
}  // namespace crashpad

namespace crash_reporter {
namespace {

void SetJavaExceptionInfo(const char* info_string) {
  static crashpad::StringAnnotation<5 * 4096> exception_info("exception_info");
  if (info_string) {
    exception_info.Set(info_string);
  } else {
    exception_info.Clear();
  }
}

void SetBuildInfoAnnotations(std::map<std::string, std::string>* annotations) {
  base::android::BuildInfo* info = base::android::BuildInfo::GetInstance();

  (*annotations)["android_build_id"] = info->android_build_id();
  (*annotations)["android_build_fp"] = info->android_build_fp();
  (*annotations)["sdk"] = base::StringPrintf("%d", info->sdk_int());
  (*annotations)["device"] = info->device();
  (*annotations)["model"] = info->model();
  (*annotations)["brand"] = info->brand();
  (*annotations)["board"] = info->board();
  (*annotations)["installer_package_name"] = info->installer_package_name();
  (*annotations)["abi_name"] = info->abi_name();
  (*annotations)["custom_themes"] = info->custom_themes();
  (*annotations)["resources_version"] = info->resources_version();
  (*annotations)["gms_core_version"] = info->gms_version_code();

  (*annotations)["package"] = std::string(info->package_name()) + " v" +
                              info->package_version_code() + " (" +
                              info->package_version_name() + ")";
}

// Constructs paths to a handler trampoline executable and a library exporting
// the symbol `CrashpadHandlerMain()`. This requires this function to be built
// into the same object exporting this symbol and the handler trampoline is
// adjacent to it.
bool GetHandlerTrampoline(std::string* handler_trampoline,
                          std::string* handler_library) {
  // The linker doesn't support loading executables passed on its command
  // line until Q.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_Q) {
    return false;
  }

  Dl_info info;
  if (dladdr(reinterpret_cast<void*>(&GetHandlerTrampoline), &info) == 0 ||
      dlsym(dlopen(info.dli_fname, RTLD_NOLOAD | RTLD_LAZY),
            "CrashpadHandlerMain") == nullptr) {
    return false;
  }

  std::string local_handler_library(info.dli_fname);

  size_t libdir_end = local_handler_library.rfind('/');
  if (libdir_end == std::string::npos) {
    return false;
  }

  std::string local_handler_trampoline(local_handler_library, 0,
                                       libdir_end + 1);
  local_handler_trampoline += "libcrashpad_handler_trampoline.so";

  handler_trampoline->swap(local_handler_trampoline);
  handler_library->swap(local_handler_library);
  return true;
}

#if defined(__arm__) && defined(__ARM_ARCH_7A__)
#define CURRENT_ABI "armeabi-v7a"
#elif defined(__arm__)
#define CURRENT_ABI "armeabi"
#elif defined(__i386__)
#define CURRENT_ABI "x86"
#elif defined(__mips__)
#define CURRENT_ABI "mips"
#elif defined(__x86_64__)
#define CURRENT_ABI "x86_64"
#elif defined(__aarch64__)
#define CURRENT_ABI "arm64-v8a"
#elif defined(__riscv) && (__riscv_xlen == 64)
#define CURRENT_ABI "riscv64"
#else
#error "Unsupported target abi"
#endif

void MakePackagePaths(std::string* classpath, std::string* libpath) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jstring> arch =
      base::android::ConvertUTF8ToJavaString(env,
                                             std::string_view(CURRENT_ABI));
  base::android::ScopedJavaLocalRef<jobjectArray> paths =
      Java_PackagePaths_makePackagePaths(env, arch);

  base::android::ConvertJavaStringToUTF8(
      env, static_cast<jstring>(env->GetObjectArrayElement(paths.obj(), 0)),
      classpath);
  base::android::ConvertJavaStringToUTF8(
      env, static_cast<jstring>(env->GetObjectArrayElement(paths.obj(), 1)),
      libpath);
}

// Copies and extends the current environment with CLASSPATH and LD_LIBRARY_PATH
// set to library paths in the APK.
bool BuildEnvironmentWithApk(bool use_64_bit,
                             std::vector<std::string>* result) {
  DCHECK(result->empty());

  std::string classpath;
  std::string library_path;
  MakePackagePaths(&classpath, &library_path);

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  static constexpr char kClasspathVar[] = "CLASSPATH";
  std::string current_classpath;
  env->GetVar(kClasspathVar, &current_classpath);
  classpath += ":" + current_classpath;

  static constexpr char kLdLibraryPathVar[] = "LD_LIBRARY_PATH";
  std::string current_library_path;
  env->GetVar(kLdLibraryPathVar, &current_library_path);
  library_path += ":" + current_library_path;

  static constexpr char kRuntimeRootVar[] = "ANDROID_RUNTIME_ROOT";
  std::string runtime_root;
  if (env->GetVar(kRuntimeRootVar, &runtime_root)) {
    library_path += ":" + runtime_root + (use_64_bit ? "/lib64" : "/lib");
  }

  result->push_back("CLASSPATH=" + classpath);
  result->push_back("LD_LIBRARY_PATH=" + library_path);
  for (char** envp = environ; *envp != nullptr; ++envp) {
    if ((strncmp(*envp, kClasspathVar, strlen(kClasspathVar)) == 0 &&
         (*envp)[strlen(kClasspathVar)] == '=') ||
        (strncmp(*envp, kLdLibraryPathVar, strlen(kLdLibraryPathVar)) == 0 &&
         (*envp)[strlen(kLdLibraryPathVar)] == '=')) {
      continue;
    }
    result->push_back(*envp);
  }

  return true;
}

const char kCrashpadJavaMain[] =
    "org.chromium.components.crash.browser.CrashpadMain";

void BuildHandlerArgs(CrashReporterClient* crash_reporter_client,
                      base::FilePath* database_path,
                      base::FilePath* metrics_path,
                      std::string* url,
                      std::map<std::string, std::string>* process_annotations,
                      std::vector<std::string>* arguments) {
  crash_reporter_client->GetCrashDumpLocation(database_path);
  crash_reporter_client->GetCrashMetricsLocation(metrics_path);

  // TODO(jperaza): Set URL for Android when Crashpad takes over report upload.
  *url = std::string();

  std::string product_name;
  std::string product_version;
  std::string channel;
  crash_reporter_client->GetProductNameAndVersion(&product_name,
                                                  &product_version, &channel);
  (*process_annotations)["prod"] = product_name;
  (*process_annotations)["ver"] = product_version;

  SetBuildInfoAnnotations(process_annotations);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Empty means stable.
  const bool allow_empty_channel = true;
#else
  const bool allow_empty_channel = false;
#endif
  if (allow_empty_channel || !channel.empty()) {
    (*process_annotations)["channel"] = channel;
  }

  (*process_annotations)["plat"] = std::string("Android");
}

bool ShouldHandleCrashAndUpdateArguments(bool write_minidump_to_database,
                                         bool write_minidump_to_log,
                                         std::vector<std::string>* arguments) {
  if (!write_minidump_to_database)
    arguments->push_back("--no-write-minidump-to-database");
  if (write_minidump_to_log)
    arguments->push_back("--write-minidump-to-log");
  return write_minidump_to_database || write_minidump_to_log;
}

bool GetHandlerPath(base::FilePath* exe_dir, base::FilePath* handler_path) {
  // There is not any normal way to package native executables in an Android
  // APK. The Crashpad handler is packaged like a loadable module, which
  // Android's APK installer expects to be named like a shared library, but it
  // is in fact a standalone executable.
  if (!base::PathService::Get(base::DIR_MODULE, exe_dir)) {
    return false;
  }
  *handler_path = exe_dir->Append("libchrome_crashpad_handler.so");
  return true;
}

bool SetLdLibraryPath(const base::FilePath& lib_path) {
#if defined(COMPONENT_BUILD)
  std::string library_path(lib_path.value());

  static constexpr char kLibraryPathVar[] = "LD_LIBRARY_PATH";
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string old_path;
  if (env->GetVar(kLibraryPathVar, &old_path)) {
    library_path.push_back(':');
    library_path.append(old_path);
  }

  if (!env->SetVar(kLibraryPathVar, library_path)) {
    return false;
  }
#endif

  return true;
}

class HandlerStarter {
  // TODO(jperaza): Currently only launching a same-bitness handler is
  // supported. The logic to build package paths, locate a handler executable,
  // and the crashpad client interface for launching a Java handler need to be
  // updated to use a specified bitness before a cross-bitness handler can be
  // used.
#if defined(ARCH_CPU_64_BITS)
  static constexpr bool kUse64Bit = true;
#else
  static constexpr bool kUse64Bit = false;
#endif

 public:
  static HandlerStarter* Get() {
    static HandlerStarter* instance = new HandlerStarter();
    return instance;
  }

  HandlerStarter(const HandlerStarter&) = delete;
  HandlerStarter& operator=(const HandlerStarter&) = delete;

  base::FilePath Initialize(bool dump_at_crash) {
    base::FilePath database_path;
    base::FilePath metrics_path;
    std::string url;
    std::map<std::string, std::string> process_annotations;
    std::vector<std::string> arguments;
    BuildHandlerArgs(GetCrashReporterClient(), &database_path, &metrics_path,
                     &url, &process_annotations, &arguments);

    base::FilePath exe_dir;
    base::FilePath handler_path;
    if (!GetHandlerPath(&exe_dir, &handler_path)) {
      return database_path;
    }

    if (crashpad::SetSanitizationInfo(GetCrashReporterClient(),
                                      &browser_sanitization_info_)) {
      arguments.push_back(base::StringPrintf("--sanitization-information=%p",
                                             &browser_sanitization_info_));
    }

    std::string browser_ptype;
    if (GetCrashReporterClient()->GetBrowserProcessType(&browser_ptype)) {
      process_annotations["ptype"] = browser_ptype;
    }

    // Don't handle SIGQUIT in the browser process on Android; the system masks
    // this and uses it for generating ART stack traces, and if it gets unmasked
    // (e.g. by a WebView app) we don't want to treat this as a crash.
    GetCrashpadClient().SetUnhandledSignals({SIGQUIT});

    if (!base::PathExists(handler_path)) {
      use_java_handler_ =
          !GetHandlerTrampoline(&handler_trampoline_, &handler_library_);
    }

    if (!ShouldHandleCrashAndUpdateArguments(
            dump_at_crash, GetCrashReporterClient()->ShouldWriteMinidumpToLog(),
            &arguments)) {
      return database_path;
    }

    if (use_java_handler_ || !handler_trampoline_.empty()) {
      std::vector<std::string> env;
      if (!BuildEnvironmentWithApk(kUse64Bit, &env)) {
        return database_path;
      }

      bool result = use_java_handler_
                        ? GetCrashpadClient().StartJavaHandlerAtCrash(
                              kCrashpadJavaMain, &env, database_path,
                              metrics_path, url, process_annotations, arguments)
                        : GetCrashpadClient().StartHandlerWithLinkerAtCrash(
                              handler_trampoline_, handler_library_, kUse64Bit,
                              &env, database_path, metrics_path, url,
                              process_annotations, arguments);
      DCHECK(result);
      return database_path;
    }

    if (!SetLdLibraryPath(exe_dir)) {
      return database_path;
    }

    bool result = GetCrashpadClient().StartHandlerAtCrash(
        handler_path, database_path, metrics_path, url, process_annotations,
        arguments);
    DCHECK(result);
    return database_path;
  }

  bool StartHandlerForClient(CrashReporterClient* client,
                             int fd,
                             bool write_minidump_to_database) {
    base::FilePath database_path;
    base::FilePath metrics_path;
    std::string url;
    std::map<std::string, std::string> process_annotations;
    std::vector<std::string> arguments;
    BuildHandlerArgs(client, &database_path, &metrics_path, &url,
                     &process_annotations, &arguments);

    base::FilePath exe_dir;
    base::FilePath handler_path;
    if (!GetHandlerPath(&exe_dir, &handler_path)) {
      return false;
    }

    if (!ShouldHandleCrashAndUpdateArguments(write_minidump_to_database,
                                             client->ShouldWriteMinidumpToLog(),
                                             &arguments)) {
      return true;
    }

    if (use_java_handler_ || !handler_trampoline_.empty()) {
      std::vector<std::string> env;
      if (!BuildEnvironmentWithApk(kUse64Bit, &env)) {
        return false;
      }

      bool result =
          use_java_handler_
              ? GetCrashpadClient().StartJavaHandlerForClient(
                    kCrashpadJavaMain, &env, database_path, metrics_path, url,
                    process_annotations, arguments, fd)
              : GetCrashpadClient().StartHandlerWithLinkerForClient(
                    handler_trampoline_, handler_library_, kUse64Bit, &env,
                    database_path, metrics_path, url, process_annotations,
                    arguments, fd);
      return result;
    }

    if (!SetLdLibraryPath(exe_dir)) {
      return false;
    }

    return GetCrashpadClient().StartHandlerForClient(
        handler_path, database_path, metrics_path, url, process_annotations,
        arguments, fd);
  }

 private:
  HandlerStarter() = default;
  ~HandlerStarter() = delete;

  crashpad::SanitizationInformation browser_sanitization_info_;
  std::string handler_trampoline_;
  std::string handler_library_;
  bool use_java_handler_ = false;
};

bool g_is_browser = false;

}  // namespace

// TODO(jperaza): This might be simplified to have both the browser and child
// processes use CRASHPAD_SIMULATE_CRASH() if CrashpadClient allows injecting
// the Chromium specific SandboxedHandler.
void DumpWithoutCrashing() {
  if (g_is_browser) {
    CRASHPAD_SIMULATE_CRASH();
  } else {
    siginfo_t siginfo;
    siginfo.si_signo = crashpad::Signals::kSimulatedSigno;
    siginfo.si_errno = 0;
    siginfo.si_code = 0;

    ucontext_t context;
    crashpad::CaptureContext(&context);

    crashpad::SandboxedHandler::Get()->HandleCrashNonFatal(siginfo.si_signo,
                                                           &siginfo, &context);
  }
}

void AllowMemoryRange(void* begin, size_t length) {
  crashpad::AllowedMemoryRanges::Singleton()->AddEntry(
      crashpad::FromPointerCast<crashpad::VMAddress>(begin),
      static_cast<crashpad::VMSize>(length));
}

namespace internal {

bool StartHandlerForClient(int fd, bool write_minidump_to_database) {
  return HandlerStarter::Get()->StartHandlerForClient(
      GetCrashReporterClient(), fd, write_minidump_to_database);
}

bool PlatformCrashpadInitialization(
    bool initial_client,
    bool browser_process,
    bool embedded_handler,
    const std::string& user_data_dir,
    const base::FilePath& exe_path,
    const std::vector<std::string>& initial_arguments,
    base::FilePath* database_path) {
  DCHECK_EQ(initial_client, browser_process);
  DCHECK(initial_arguments.empty());

  // Not used on Android.
  DCHECK(!embedded_handler);
  DCHECK(exe_path.empty());

  g_is_browser = browser_process;

  base::android::SetJavaExceptionCallback(SetJavaExceptionInfo);

  CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
  bool dump_at_crash = true;
  unsigned int dump_percentage =
      crash_reporter_client->GetCrashDumpPercentage();
  if (dump_percentage < 100 &&
      static_cast<unsigned int>(base::RandInt(0, 99)) >= dump_percentage) {
    dump_at_crash = false;
  }

  // In the not-large-dumps case, record enough extra memory to be able to save
  // dereferenced memory from all registers on the crashing thread. Crashpad may
  // save 512-bytes per register, and the largest register set (not including
  // stack pointers) is ARM64 with 32 registers. Hence, 16 KiB.
  const uint32_t indirect_memory_limit =
      crash_reporter_client->GetShouldDumpLargerDumps() ? 4 * 1024 * 1024
                                                        : 16 * 1024;
  crashpad::CrashpadInfo::GetCrashpadInfo()
      ->set_gather_indirectly_referenced_memory(crashpad::TriState::kEnabled,
                                                indirect_memory_limit);

  if (browser_process) {
    HandlerStarter* starter = HandlerStarter::Get();
    *database_path = starter->Initialize(dump_at_crash);
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
    // Handler gets called in SignalHandler::HandleOrReraiseSignal() after
    // reporting the crash.
    crashpad::CrashpadClient::SetLastChanceExceptionHandler(
        partition_alloc::PermissiveMte::HandleCrash);
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)
    return true;
  }

  crashpad::SandboxedHandler* handler = crashpad::SandboxedHandler::Get();
  bool result = handler->Initialize(dump_at_crash);
  DCHECK(result);

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
  handler->SetLastChanceExceptionHandler(
      partition_alloc::PermissiveMte::HandleCrash);
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)

  *database_path = base::FilePath();
  return true;
}

}  // namespace internal

}  // namespace crash_reporter
