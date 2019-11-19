// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/loader/nacl_listener.h"
#include "base/bind.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <utility>

#if defined(OS_POSIX)
#include <unistd.h>
#endif

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/message_loop/message_pump_type.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl.mojom.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_service.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/loader/nacl_ipc_adapter.h"
#include "components/nacl/loader/nacl_validation_db.h"
#include "components/nacl/loader/nacl_validation_query.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "native_client/src/public/chrome_main.h"
#include "native_client/src/public/nacl_app.h"
#include "native_client/src/public/nacl_desc.h"

#if defined(OS_LINUX)
#include "services/service_manager/zygote/common/common_sandbox_support_linux.h"
#endif

#if defined(OS_POSIX)
#include "base/posix/eintr_wrapper.h"
#endif

#if defined(OS_WIN)
#include <io.h>

#include "content/public/common/sandbox_init.h"
#endif

namespace {

NaClListener* g_listener;

void FatalLogHandler(const char* data, size_t bytes) {
  // We use uint32_t rather than size_t for the case when the browser and NaCl
  // processes are a mix of 32-bit and 64-bit processes.
  uint32_t copy_bytes = std::min<uint32_t>(static_cast<uint32_t>(bytes),
                                           nacl::kNaClCrashInfoMaxLogSize);

  // We copy the length of the crash data to the start of the shared memory
  // segment so we know how much to copy.
  memcpy(g_listener->crash_info_shmem_memory(), &copy_bytes, sizeof(uint32_t));

  memcpy((char*)g_listener->crash_info_shmem_memory() + sizeof(uint32_t),
         data,
         copy_bytes);
}

void LoadStatusCallback(int load_status) {
  g_listener->trusted_listener()->renderer_host()->ReportLoadStatus(
      static_cast<NaClErrorCode>(load_status));
}

#if defined(OS_WIN)
int AttachDebugExceptionHandler(const void* info, size_t info_size) {
  std::string info_string(reinterpret_cast<const char*>(info), info_size);
  bool result = false;
  if (!g_listener->Send(new NaClProcessMsg_AttachDebugExceptionHandler(
           info_string, &result)))
    return false;
  return result;
}

void DebugStubPortSelectedHandler(uint16_t port) {
  g_listener->Send(new NaClProcessHostMsg_DebugStubPortSelected(port));
}

#endif

// Creates the PPAPI IPC channel between the NaCl IRT and the host
// (browser/renderer) process, and starts to listen it on the thread where
// the given task runner runs.
// Also, creates and sets the corresponding NaClDesc to the given nap with
// the FD #.
void SetUpIPCAdapter(
    IPC::ChannelHandle* handle,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    struct NaClApp* nap,
    int nacl_fd,
    NaClIPCAdapter::ResolveFileTokenCallback resolve_file_token_cb,
    NaClIPCAdapter::OpenResourceCallback open_resource_cb) {
  mojo::MessagePipe pipe;
  scoped_refptr<NaClIPCAdapter> ipc_adapter(
      new NaClIPCAdapter(pipe.handle0.release(), task_runner,
                         resolve_file_token_cb, open_resource_cb));
  ipc_adapter->ConnectChannel();
  *handle = pipe.handle1.release();

  // Pass a NaClDesc to the untrusted side. This will hold a ref to the
  // NaClIPCAdapter.
  NaClAppSetDesc(nap, nacl_fd, ipc_adapter->MakeNaClDesc());
}

}  // namespace

class BrowserValidationDBProxy : public NaClValidationDB {
 public:
  explicit BrowserValidationDBProxy(NaClListener* listener)
      : listener_(listener) {
  }

  bool QueryKnownToValidate(const std::string& signature) override {
    // Initialize to false so that if the Send fails to write to the return
    // value we're safe.  For example if the message is (for some reason)
    // dispatched as an async message the return parameter will not be written.
    bool result = false;
    if (!listener_->Send(new NaClProcessMsg_QueryKnownToValidate(signature,
                                                                 &result))) {
      LOG(ERROR) << "Failed to query NaCl validation cache.";
      result = false;
    }
    return result;
  }

  void SetKnownToValidate(const std::string& signature) override {
    // Caching is optional: NaCl will still work correctly if the IPC fails.
    if (!listener_->Send(new NaClProcessMsg_SetKnownToValidate(signature))) {
      LOG(ERROR) << "Failed to update NaCl validation cache.";
    }
  }

 private:
  // The listener never dies, otherwise this might be a dangling reference.
  NaClListener* listener_;
};

NaClListener::NaClListener()
    : shutdown_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED),
      io_thread_("NaCl_IOThread"),
#if defined(OS_LINUX)
      prereserved_sandbox_size_(0),
#endif
#if defined(OS_POSIX)
      number_of_cores_(-1),  // unknown/error
#endif
      is_started_(false) {
  io_thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  DCHECK(g_listener == NULL);
  g_listener = this;
}

NaClListener::~NaClListener() {
  NOTREACHED();
  shutdown_event_.Signal();
  g_listener = NULL;
}

bool NaClListener::Send(IPC::Message* msg) {
  DCHECK(!!main_task_runner_);
  if (main_task_runner_->BelongsToCurrentThread()) {
    // This thread owns the channel.
    return channel_->Send(msg);
  }
  // This thread does not own the channel.
  return filter_->Send(msg);
}

// The NaClProcessMsg_ResolveFileTokenAsyncReply message must be
// processed in a MessageFilter so it can be handled on the IO thread.
// The main thread used by NaClListener is busy in
// NaClChromeMainAppStart(), so it can't be used for servicing messages.
class FileTokenMessageFilter : public IPC::MessageFilter {
 public:
  bool OnMessageReceived(const IPC::Message& msg) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(FileTokenMessageFilter, msg)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_ResolveFileTokenReply,
                          OnResolveFileTokenReply)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void OnResolveFileTokenReply(
      uint64_t token_lo,
      uint64_t token_hi,
      IPC::PlatformFileForTransit ipc_fd,
      base::FilePath file_path) {
    CHECK(g_listener);
    g_listener->OnFileTokenResolved(token_lo, token_hi, ipc_fd, file_path);
  }
 private:
  ~FileTokenMessageFilter() override {}
};

void NaClListener::Listen() {
  NaClService service(io_thread_.task_runner());
  channel_ = IPC::SyncChannel::Create(this, io_thread_.task_runner().get(),
                                      base::ThreadTaskRunnerHandle::Get(),
                                      &shutdown_event_);
  filter_ = channel_->CreateSyncMessageFilter();
  channel_->AddFilter(new FileTokenMessageFilter());
  channel_->Init(service.TakeChannelPipe().release(), IPC::Channel::MODE_CLIENT,
                 true);
  main_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  base::RunLoop().Run();
}

#if defined(OS_LINUX)
// static
int NaClListener::MakeSharedMemorySegment(size_t length, int executable) {
  return service_manager::SharedMemoryIPCSupport::MakeSharedMemorySegment(
      length, executable);
}
#endif

bool NaClListener::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NaClListener, msg)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_AddPrefetchedResource,
                          OnAddPrefetchedResource)
      IPC_MESSAGE_HANDLER(NaClProcessMsg_Start, OnStart)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool NaClListener::OnOpenResource(
    const IPC::Message& msg,
    const std::string& key,
    NaClIPCAdapter::OpenResourceReplyCallback cb) {
  // This callback is executed only on |io_thread_| with NaClIPCAdapter's
  // |lock_| not being held.
  DCHECK(!cb.is_null());
  auto it = prefetched_resource_files_.find(key);

  if (it != prefetched_resource_files_.end()) {
    // Fast path for prefetched FDs.
    IPC::PlatformFileForTransit file = it->second.first;
    base::FilePath path = it->second.second;
    prefetched_resource_files_.erase(it);
    // A pre-opened resource descriptor is available. Run the reply callback
    // and return true.
    cb.Run(msg, file, path);
    return true;
  }

  // Return false to fall back to the slow path. Let NaClIPCAdapter issue an
  // IPC to the renderer.
  return false;
}

void NaClListener::OnAddPrefetchedResource(
    const nacl::NaClResourcePrefetchResult& prefetched_resource_file) {
  DCHECK(!is_started_);
  if (is_started_)
    return;
  bool result = prefetched_resource_files_.insert(std::make_pair(
      prefetched_resource_file.file_key,
      std::make_pair(
          prefetched_resource_file.file,
          prefetched_resource_file.file_path_metadata))).second;
  if (!result) {
    LOG(FATAL) << "Duplicated open_resource key: "
               << prefetched_resource_file.file_key;
  }
}

void NaClListener::OnStart(nacl::NaClStartParams params) {
  is_started_ = true;
#if defined(OS_LINUX) || defined(OS_MACOSX)
  int urandom_fd = HANDLE_EINTR(dup(base::GetUrandomFD()));
  if (urandom_fd < 0) {
    LOG(FATAL) << "Failed to dup() the urandom FD";
  }
  NaClChromeMainSetUrandomFd(urandom_fd);
#endif
  struct NaClApp* nap = NULL;
  NaClChromeMainInit();

  CHECK(params.crash_info_shmem_region.IsValid());
  crash_info_shmem_mapping_ = params.crash_info_shmem_region.Map();
  base::ReadOnlySharedMemoryRegion ro_shmem_region =
      base::WritableSharedMemoryRegion::ConvertToReadOnly(
          std::move(params.crash_info_shmem_region));
  CHECK(crash_info_shmem_mapping_.IsValid());
  CHECK(ro_shmem_region.IsValid());
  NaClSetFatalErrorCallback(&FatalLogHandler);

  nap = NaClAppCreate();
  if (nap == NULL) {
    LOG(FATAL) << "NaClAppCreate() failed";
  }

  IPC::ChannelHandle browser_handle;
  IPC::ChannelHandle ppapi_renderer_handle;
  IPC::ChannelHandle manifest_service_handle;

  // Create the PPAPI IPC channels between the NaCl IRT and the host
  // (browser/renderer) processes. The IRT uses these channels to
  // communicate with the host and to initialize the IPC dispatchers.
  SetUpIPCAdapter(&browser_handle, io_thread_.task_runner(), nap,
                  NACL_CHROME_DESC_BASE,
                  NaClIPCAdapter::ResolveFileTokenCallback(),
                  NaClIPCAdapter::OpenResourceCallback());
  SetUpIPCAdapter(&ppapi_renderer_handle, io_thread_.task_runner(), nap,
                  NACL_CHROME_DESC_BASE + 1,
                  NaClIPCAdapter::ResolveFileTokenCallback(),
                  NaClIPCAdapter::OpenResourceCallback());
  SetUpIPCAdapter(
      &manifest_service_handle, io_thread_.task_runner(), nap,
      NACL_CHROME_DESC_BASE + 2,
      base::Bind(&NaClListener::ResolveFileToken, base::Unretained(this)),
      base::Bind(&NaClListener::OnOpenResource, base::Unretained(this)));

  mojo::PendingRemote<nacl::mojom::NaClRendererHost> renderer_host;
  if (!Send(new NaClProcessHostMsg_PpapiChannelsCreated(
          browser_handle, ppapi_renderer_handle,
          renderer_host.InitWithNewPipeAndPassReceiver().PassPipe().release(),
          manifest_service_handle, ro_shmem_region)))
    LOG(FATAL) << "Failed to send IPC channel handle to NaClProcessHost.";

  trusted_listener_ = std::make_unique<NaClTrustedListener>(
      std::move(renderer_host), io_thread_.task_runner().get());
  struct NaClChromeMainArgs* args = NaClChromeMainArgsCreate();
  if (args == NULL) {
    LOG(FATAL) << "NaClChromeMainArgsCreate() failed";
  }

#if defined(OS_POSIX)
  args->number_of_cores = number_of_cores_;
#endif

#if defined(OS_LINUX)
  args->create_memory_object_func = &MakeSharedMemorySegment;
#endif

  DCHECK(params.process_type != nacl::kUnknownNaClProcessType);
  CHECK(params.irt_handle != IPC::InvalidPlatformFileForTransit());
  base::PlatformFile irt_handle =
      IPC::PlatformFileForTransitToPlatformFile(params.irt_handle);

#if defined(OS_WIN)
  args->irt_fd = _open_osfhandle(reinterpret_cast<intptr_t>(irt_handle),
                                 _O_RDONLY | _O_BINARY);
  if (args->irt_fd < 0) {
    LOG(FATAL) << "_open_osfhandle() failed";
  }
#else
  args->irt_fd = irt_handle;
#endif

  if (params.validation_cache_enabled) {
    // SHA256 block size.
    CHECK_EQ(params.validation_cache_key.length(), (size_t) 64);
    // The cache structure is not freed and exists until the NaCl process exits.
    args->validation_cache = CreateValidationCache(
        new BrowserValidationDBProxy(this), params.validation_cache_key,
        params.version);
  }

  args->enable_debug_stub = params.enable_debug_stub;

  // Now configure parts that depend on process type.
  // Start with stricter settings.
  args->enable_exception_handling = 0;
  args->enable_dyncode_syscalls = 0;
  // pnacl_mode=1 mostly disables things (IRT interfaces and syscalls).
  args->pnacl_mode = 1;
  // Bound the initial nexe's code segment size under PNaCl to reduce the
  // chance of a code spraying attack succeeding (see
  // https://code.google.com/p/nativeclient/issues/detail?id=3572).
  // We can't apply this arbitrary limit outside of PNaCl because it might
  // break existing NaCl apps, and this limit is only useful if the dyncode
  // syscalls are disabled.
  args->initial_nexe_max_code_bytes = 64 << 20;  // 64 MB.

  if (params.process_type == nacl::kNativeNaClProcessType) {
    args->enable_exception_handling = 1;
    args->enable_dyncode_syscalls = 1;
    args->pnacl_mode = 0;
    args->initial_nexe_max_code_bytes = 0;
  } else if (params.process_type == nacl::kPNaClTranslatorProcessType) {
    args->pnacl_mode = 0;
  }

#if defined(OS_POSIX)
  args->debug_stub_server_bound_socket_fd =
      IPC::PlatformFileForTransitToPlatformFile(
          params.debug_stub_server_bound_socket);
#endif
#if defined(OS_WIN)
  args->attach_debug_exception_handler_func = AttachDebugExceptionHandler;
  args->debug_stub_server_port_selected_handler_func =
      DebugStubPortSelectedHandler;
#endif
  args->load_status_handler_func = LoadStatusCallback;
#if defined(OS_LINUX)
  args->prereserved_sandbox_size = prereserved_sandbox_size_;
#endif

  base::PlatformFile nexe_file = IPC::PlatformFileForTransitToPlatformFile(
      params.nexe_file);
  std::string file_path_str = params.nexe_file_path_metadata.AsUTF8Unsafe();
  args->nexe_desc = NaClDescCreateWithFilePathMetadata(nexe_file,
                                                       file_path_str.c_str());

  int exit_status;
  if (!NaClChromeMainStart(nap, args, &exit_status))
    NaClExit(1);

  // Report the plugin's exit status if the application started successfully.
  trusted_listener_->renderer_host()->ReportExitStatus(exit_status);
  NaClExit(exit_status);
}

void NaClListener::ResolveFileToken(
    uint64_t token_lo,
    uint64_t token_hi,
    base::Callback<void(IPC::PlatformFileForTransit, base::FilePath)> cb) {
  if (!Send(new NaClProcessMsg_ResolveFileToken(token_lo, token_hi))) {
    cb.Run(IPC::PlatformFileForTransit(), base::FilePath());
    return;
  }
  resolved_cb_ = cb;
}

void NaClListener::OnFileTokenResolved(
    uint64_t token_lo,
    uint64_t token_hi,
    IPC::PlatformFileForTransit ipc_fd,
    base::FilePath file_path) {
  resolved_cb_.Run(ipc_fd, file_path);
  resolved_cb_.Reset();
}
