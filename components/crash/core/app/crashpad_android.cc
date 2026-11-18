// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/app/crashpad.h"

#include <dlfcn.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/system_properties.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "base/android/android_info.h"
#include "base/android/apk_info.h"
#include "base/android/device_info.h"
#include "base/android/java_exception_reporter.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_view_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "components/crash/core/app/shared_memory_user_stream_args.h"
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
    restore_previous_handler_ =
        base::android::android_info::sdk_int() >=
            base::android::android_info::SDK_VERSION_OREO ||
        base::android::android_info::build_type() == "eng" ||
        base::android::android_info::build_type() == "userdebug";

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
    *reinterpret_cast<int*>(UNSAFE_TODO(CMSG_DATA(cmsg))) =
        handlers_socket.get();

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

// When enabled, 32-bit browser processes launch a 64-bit Crashpad handler for
// child processes when the package's primary CPU ABI is 64-bit. A 32-bit
// Crashpad handler cannot ptrace a 64-bit process (e.g., a 64-bit WebView
// renderer hosted by a 32-bit app), so launching a 64-bit handler allows
// minidumps to be captured for those crashes.
BASE_FEATURE(kAlwaysUse64BitCrashHandlerIfAvailable,
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

constexpr char kHandlerTrampoline[] = "libcrashpad_handler_trampoline.so";

void SetJavaExceptionInfo(const char* info_string) {
  static crashpad::StringAnnotation<5 * 4096> exception_info("exception_info");
  if (info_string) {
    exception_info.Set(info_string);
  } else {
    exception_info.Clear();
  }
}

void SetBuildInfoAnnotations(std::map<std::string, std::string>* annotations) {
  (*annotations)["android_build_id"] =
      base::android::android_info::android_build_id();
  (*annotations)["android_build_fp"] =
      base::android::android_info::android_build_fp();
  (*annotations)["sdk"] =
      base::StringPrintf("%d", base::android::android_info::sdk_int());
  (*annotations)["device"] = base::android::android_info::device();
  (*annotations)["model"] = base::android::android_info::model();
  (*annotations)["brand"] = base::android::android_info::brand();
  (*annotations)["board"] = base::android::android_info::board();
  (*annotations)["installer_package_name"] =
      base::android::apk_info::installer_package_name();
  (*annotations)["abi_name"] = base::android::android_info::abi_name();
  (*annotations)["gms_core_version"] =
      base::android::device_info::gms_version_code();

  (*annotations)["package"] =
      std::string(base::android::apk_info::package_name()) + " v" +
      base::android::apk_info::package_version_code() + " (" +
      base::android::apk_info::package_version_name() + ")";
}

const char* GetTargetAbi(bool is_64_bit) {
#if defined(__arm__) || defined(__aarch64__)
  return is_64_bit ? "arm64-v8a" : "armeabi-v7a";
#elif defined(__i386__) || defined(__x86_64__)
  return is_64_bit ? "x86_64" : "x86";
#elif defined(__mips__) || defined(__mips64__)
  return is_64_bit ? "mips64" : "mips";
#elif defined(__riscv) && (__riscv_xlen == 64)
  return "riscv64";
#else
#error "Unsupported target abi"
#endif
}

bool IsPrimaryCpuAbi64Bit() {
  const std::string& primary_abi = base::android::apk_info::primary_cpu_abi();
  if (primary_abi.empty()) {
    return false;
  }
  std::array<char, PROP_VALUE_MAX> abilist64;
  int len = __system_property_get("ro.product.cpu.abilist64", abilist64.data());
  if (len <= 0) {
    return false;
  }
  std::string_view abilist64_view = base::as_string_view(
      base::span(abilist64).first(static_cast<size_t>(len)));
  std::vector<std::string_view> supported_64_bit_abis = base::SplitStringPiece(
      abilist64_view, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return std::ranges::contains(supported_64_bit_abis, primary_abi);
}

void MakePackagePaths(bool use_64_bit,
                      bool is_same_bitness,
                      std::string* classpath,
                      std::string* libpath) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jstring> arch =
      base::android::ConvertUTF8ToJavaString(
          env, std::string_view(GetTargetAbi(use_64_bit)));
  base::android::ScopedJavaLocalRef<jobjectArray> paths =
      Java_PackagePaths_makePackagePaths(env, arch, is_same_bitness);

  base::android::ConvertJavaStringToUTF8(
      env, static_cast<jstring>(env->GetObjectArrayElement(paths.obj(), 0)),
      classpath);
  base::android::ConvertJavaStringToUTF8(
      env, static_cast<jstring>(env->GetObjectArrayElement(paths.obj(), 1)),
      libpath);
}

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

  ProductInfo product_info;
  crash_reporter_client->GetProductInfo(&product_info);
  (*process_annotations)["prod"] = std::string(product_info.product_name());
  (*process_annotations)["ver"] = std::string(product_info.version());

  SetBuildInfoAnnotations(process_annotations);

  std::string_view channel = product_info.channel();
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Empty means stable.
  const bool allow_empty_channel = true;
#else
  const bool allow_empty_channel = false;
#endif
  if (allow_empty_channel || !channel.empty()) {
    (*process_annotations)["channel"] = std::string(channel);
  }

  (*process_annotations)["plat"] = std::string("Android");
  for (auto& [key, value] :
       crash_reporter_client->GetExtraProcessAnnotations()) {
    (*process_annotations)[key] = std::move(value);
  }
}

bool ShouldHandleCrashAndUpdateArguments(bool write_minidump_to_database,
                                         bool write_minidump_to_log,
                                         std::vector<std::string>* arguments) {
  if (!write_minidump_to_database) {
    arguments->push_back("--no-write-minidump-to-database");
  }
  if (write_minidump_to_log) {
    arguments->push_back("--write-minidump-to-log");
  }
  return write_minidump_to_database || write_minidump_to_log;
}

bool Get64BitHandlerTrampoline(const std::string& browser_library,
                               std::string* handler_trampoline,
                               std::string* handler_library) {
  base::FilePath library_path(browser_library);
  base::FilePath libdir = library_path.DirName();
  if (libdir.BaseName().value() != GetTargetAbi(/*is_64_bit=*/false)) {
    return false;
  }

  base::FilePath libdir_64 =
      libdir.DirName().AppendASCII(GetTargetAbi(/*is_64_bit=*/true));
  *handler_trampoline = libdir_64.AppendASCII(kHandlerTrampoline).value();
  *handler_library = libdir_64.Append(library_path.BaseName()).value();
  return true;
}

class HandlerStarter {
#if defined(ARCH_CPU_64_BITS)
  static constexpr bool kBrowserIs64Bit = true;
#else
  static constexpr bool kBrowserIs64Bit = false;
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

    internal::GetHandlerTrampoline(&browser_trampoline_, &browser_library_);

    user_streams_ =
        GetCrashReporterClient()->GetUserStreamSharedMemoryRegions();
    for (const auto& region : user_streams_) {
      if (region.IsValid()) {
        // Those handles are kept open in the browser process throughout its
        // lifetime and passed to the handler. Set `close-on-exec` to avoid
        // passing them to other child processes.
        base::SetCloseOnExec(region.GetPlatformHandle());
      }
    }

    if (!ShouldHandleCrashAndUpdateArguments(
            dump_at_crash, GetCrashReporterClient()->ShouldWriteMinidumpToLog(),
            &arguments)) {
      return database_path;
    }

    if (!browser_trampoline_.empty()) {
      std::vector<std::string> env =
          internal::BuildEnvironmentWithApk(kBrowserIs64Bit);
      std::set<crashpad::FileHandle> preserve_handles;
      internal::AppendSharedMemoryUserStreamArgs(user_streams_, &arguments,
                                                 &preserve_handles);

      bool result = GetCrashpadClient().StartHandlerWithLinkerAtCrash(
          browser_trampoline_, browser_library_, kBrowserIs64Bit, &env,
          database_path, metrics_path, url, process_annotations, arguments,
          preserve_handles);
      DCHECK(result);
      return database_path;
    }

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

    if (!ShouldHandleCrashAndUpdateArguments(write_minidump_to_database,
                                             client->ShouldWriteMinidumpToLog(),
                                             &arguments)) {
      return true;
    }

    std::string handler_trampoline = browser_trampoline_;
    std::string handler_library = browser_library_;
    bool use_64_bit = kBrowserIs64Bit;
    if (!kBrowserIs64Bit && !handler_trampoline.empty() &&
        IsPrimaryCpuAbi64Bit() &&
        base::FeatureList::IsEnabled(kAlwaysUse64BitCrashHandlerIfAvailable)) {
      use_64_bit = Get64BitHandlerTrampoline(
          browser_library_, &handler_trampoline, &handler_library);
    }

    if (!handler_trampoline.empty()) {
      std::vector<std::string> env =
          internal::BuildEnvironmentWithApk(use_64_bit);
      std::set<crashpad::FileHandle> preserve_handles;
      internal::AppendSharedMemoryUserStreamArgs(user_streams_, &arguments,
                                                 &preserve_handles);

      return GetCrashpadClient().StartHandlerWithLinkerForClient(
          handler_trampoline, handler_library, use_64_bit, &env, database_path,
          metrics_path, url, process_annotations, arguments, fd,
          preserve_handles);
    }

    return false;
  }

 private:
  HandlerStarter() = default;
  ~HandlerStarter() = delete;

  crashpad::SanitizationInformation browser_sanitization_info_;
  std::string browser_trampoline_;
  std::string browser_library_;
  std::vector<base::ReadOnlySharedMemoryRegion> user_streams_;
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

bool GetHandlerTrampoline(std::string* handler_trampoline,
                          std::string* handler_library) {
  Dl_info info;
  CHECK(dladdr(reinterpret_cast<void*>(&GetHandlerTrampoline), &info));
  std::string local_handler_library(info.dli_fname);

  // We'll just fail when launching the handler if this fails, but nicer to
  // catch it early when DCHECK_IS_ON().
#if DCHECK_IS_ON()
  void* handle = dlopen(info.dli_fname, RTLD_NOLOAD | RTLD_LAZY);
  bool has_main = dlsym(handle, "CrashpadHandlerMain") != nullptr;
  dlclose(handle);
  if (!has_main) {
    NOTREACHED() << local_handler_library << " is missing CrashpadHandlerMain";
  }
#endif

  base::FilePath handler_library_path(info.dli_fname);
  base::FilePath libdir = handler_library_path.DirName();
  base::FilePath handler_trampoline_path =
      libdir.AppendASCII(kHandlerTrampoline);

  *handler_trampoline = handler_trampoline_path.value();
  *handler_library = handler_library_path.value();
  return true;
}

std::vector<std::string> BuildEnvironmentWithApk(bool use_64_bit) {
#if defined(ARCH_CPU_64_BITS)
  constexpr bool kIs64BitProcess = true;
#else
  constexpr bool kIs64BitProcess = false;
#endif
  const bool is_same_bitness = (use_64_bit == kIs64BitProcess);

  std::string classpath;
  std::string library_path;
  // CLASSPATH can just inherit existing values once Trichrome is removed.
  MakePackagePaths(use_64_bit, is_same_bitness, &classpath, &library_path);

  std::unique_ptr<base::Environment> env(base::Environment::Create());
  static constexpr char kClasspathVar[] = "CLASSPATH";
  std::optional<std::string> current_classpath = env->GetVar(kClasspathVar);
  if (current_classpath.has_value()) {
    classpath += ":" + current_classpath.value();
  }

  static constexpr char kLdLibraryPathVar[] = "LD_LIBRARY_PATH";
  if (is_same_bitness) {
    std::optional<std::string> current_library_path =
        env->GetVar(kLdLibraryPathVar);
    if (current_library_path.has_value()) {
      base::StrAppend(&library_path, {library_path.empty() ? "" : ":",
                                      current_library_path.value()});
    }
  }

  static constexpr char kRuntimeRootVar[] = "ANDROID_RUNTIME_ROOT";
  std::optional<std::string> runtime_root = env->GetVar(kRuntimeRootVar);
  if (runtime_root.has_value()) {
    base::StrAppend(&library_path,
                    {library_path.empty() ? "" : ":", runtime_root.value(),
                     use_64_bit ? "/lib64" : "/lib"});
  }

  std::vector<std::string> result;
  result.push_back("CLASSPATH=" + classpath);
  result.push_back("LD_LIBRARY_PATH=" + library_path);
  auto is_env_var = [](std::string_view entry, std::string_view var) {
    return entry.starts_with(var) && entry.substr(var.size()).starts_with('=');
  };
  for (std::string& entry : env->GetAllVars()) {
    static constexpr char kLdPreloadVar[] = "LD_PRELOAD";

    // Omit LD_PRELOAD when launching a cross-bitness handler, as any preloaded
    // library paths from the current process will have the wrong ELF class and
    // cause the target linker to abort on startup.
    if (is_env_var(entry, kClasspathVar) ||
        is_env_var(entry, kLdLibraryPathVar) ||
        (!is_same_bitness && is_env_var(entry, kLdPreloadVar))) {
      continue;
    }
    result.push_back(std::move(entry));
  }

  return result;
}

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
    const std::vector<base::FilePath>& attachments,
    base::FilePath* database_path) {
  DCHECK_EQ(initial_client, browser_process);
  DCHECK(initial_arguments.empty());
  DCHECK(attachments.empty());

  // Not used on Android.
  DCHECK(!embedded_handler);
  DCHECK(exe_path.empty());

  g_is_browser = browser_process;

  base::android::SetJavaExceptionCallback(SetJavaExceptionInfo);

  CrashReporterClient* crash_reporter_client = GetCrashReporterClient();
  bool dump_at_crash = true;
  unsigned int dump_percentage =
      crash_reporter_client->GetCrashDumpPercentage();
  if (dump_percentage < 100 && static_cast<unsigned int>(base::RandIntInclusive(
                                   0, 99)) >= dump_percentage) {
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

DEFINE_JNI(PackagePaths)
