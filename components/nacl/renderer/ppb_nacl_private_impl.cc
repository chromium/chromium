// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <numeric>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "components/nacl/common/nacl_messages.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/renderer/file_downloader.h"
#include "components/nacl/renderer/histogram.h"
#include "components/nacl/renderer/json_manifest.h"
#include "components/nacl/renderer/manifest_downloader.h"
#include "components/nacl/renderer/manifest_service_channel.h"
#include "components/nacl/renderer/nexe_load_manager.h"
#include "components/nacl/renderer/platform_info.h"
#include "components/nacl/renderer/pnacl_translation_resource_host.h"
#include "components/nacl/renderer/ppb_nacl_private.h"
#include "components/nacl/renderer/progress_event.h"
#include "components/nacl/renderer/trusted_plugin_channel.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/data_url.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/enter.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"

namespace nacl {
namespace {

// The pseudo-architecture used to indicate portable native client.
const char* const kPortableArch = "portable";

// The base URL for resources used by the PNaCl translator processes.
const char* kPNaClTranslatorBaseUrl = "chrome://pnacl-translator/";

base::LazyInstance<scoped_refptr<PnaclTranslationResourceHost>>::
    DestructorAtExit g_pnacl_resource_host = LAZY_INSTANCE_INITIALIZER;

bool InitializePnaclResourceHost() {
  // Must run on the main thread.
  content::RenderThread* render_thread = content::RenderThread::Get();
  if (!render_thread)
    return false;
  if (!g_pnacl_resource_host.Get().get()) {
    g_pnacl_resource_host.Get() =
        new PnaclTranslationResourceHost(render_thread->GetIOTaskRunner());
    render_thread->AddFilter(g_pnacl_resource_host.Get().get());
  }
  return true;
}

bool CanOpenViaFastPath(content::PepperPluginInstance* plugin_instance,
                        const GURL& gurl) {
  // Fast path only works for installed file URLs.
  if (!gurl.SchemeIs("chrome-extension"))
    return PP_kInvalidFileHandle;

  // IMPORTANT: Make sure the document can request the given URL. If we don't
  // check, a malicious app could probe the extension system. This enforces a
  // same-origin policy which prevents the app from requesting resources from
  // another app.
  blink::WebSecurityOrigin security_origin =
      plugin_instance->GetContainer()->GetDocument().GetSecurityOrigin();
  return security_origin.CanRequest(gurl);
}

// This contains state that is produced by LaunchSelLdr() and consumed
// by StartPpapiProxy().
struct InstanceInfo {
  InstanceInfo() : plugin_pid(base::kNullProcessId), plugin_child_id(0) {}
  GURL url;
  ppapi::PpapiPermissions permissions;
  base::ProcessId plugin_pid;
  int plugin_child_id;
  IPC::ChannelHandle channel_handle;
};

class NaClPluginInstance {
 public:
  explicit NaClPluginInstance(PP_Instance instance)
      : nexe_load_manager(instance), pexe_size(0) {}
  ~NaClPluginInstance() {
    // Make sure that we do not leak a mojo handle if the NaCl loader
    // process never called ppapi_start() to initialize PPAPI.
    if (instance_info) {
      DCHECK(instance_info->channel_handle.is_mojo_channel_handle());
      instance_info->channel_handle.mojo_handle.Close();
    }
  }

  NexeLoadManager nexe_load_manager;
  std::unique_ptr<JsonManifest> json_manifest;
  std::unique_ptr<InstanceInfo> instance_info;

  // When translation is complete, this records the size of the pexe in
  // bytes so that it can be reported in a later load event.
  uint64_t pexe_size;
};

typedef std::unordered_map<PP_Instance, std::unique_ptr<NaClPluginInstance>>
    InstanceMap;
base::LazyInstance<InstanceMap>::DestructorAtExit g_instance_map =
    LAZY_INSTANCE_INITIALIZER;

NaClPluginInstance* GetNaClPluginInstance(PP_Instance instance) {
  InstanceMap& map = g_instance_map.Get();
  auto iter = map.find(instance);
  if (iter == map.end())
    return NULL;
  return iter->second.get();
}

NexeLoadManager* GetNexeLoadManager(PP_Instance instance) {
  NaClPluginInstance* nacl_plugin_instance = GetNaClPluginInstance(instance);
  if (!nacl_plugin_instance)
    return NULL;
  return &nacl_plugin_instance->nexe_load_manager;
}

JsonManifest* GetJsonManifest(PP_Instance instance) {
  NaClPluginInstance* nacl_plugin_instance = GetNaClPluginInstance(instance);
  if (!nacl_plugin_instance)
    return NULL;
  return nacl_plugin_instance->json_manifest.get();
}

static const PP_NaClFileInfo kInvalidNaClFileInfo = {
    PP_kInvalidFileHandle,
    0,  // token_lo
    0,  // token_hi
};

int GetFrameRoutingID(PP_Instance instance) {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());
  content::RendererPpapiHost* host =
      content::RendererPpapiHost::GetForPPInstance(instance);
  if (!host)
    return 0;
  auto* render_frame = host->GetRenderFrameForInstance(instance);
  if (!render_frame)
    return 0;
  return render_frame->GetRoutingID();
}

// Returns whether the channel_handle is valid or not.
bool IsValidChannelHandle(const IPC::ChannelHandle& channel_handle) {
  DCHECK(channel_handle.is_mojo_channel_handle());
  return channel_handle.is_mojo_channel_handle();
}

void PostPPCompletionCallback(PP_CompletionCallback callback,
                              int32_t status) {
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE, base::BindOnce(callback.func, callback.user_data, status));
}

bool ManifestResolveKey(PP_Instance instance,
                        bool is_helper_process,
                        const std::string& key,
                        std::string* full_url,
                        PP_PNaClOptions* pnacl_options);

typedef base::OnceCallback<void(int32_t, const PP_NaClFileInfo&)>
    DownloadFileCallback;

void DownloadFile(PP_Instance instance,
                  const std::string& url,
                  DownloadFileCallback callback);

PP_Bool StartPpapiProxy(PP_Instance instance);

// Thin adapter from PPP_ManifestService to ManifestServiceChannel::Delegate.
// Note that user_data is managed by the caller of LaunchSelLdr. Please see
// also PP_ManifestService's comment for more details about resource
// management.
class ManifestServiceProxy : public ManifestServiceChannel::Delegate {
 public:
  ManifestServiceProxy(PP_Instance pp_instance, NaClAppProcessType process_type)
      : pp_instance_(pp_instance), process_type_(process_type) {}

  ManifestServiceProxy(const ManifestServiceProxy&) = delete;
  ManifestServiceProxy& operator=(const ManifestServiceProxy&) = delete;

  ~ManifestServiceProxy() override = default;

  void StartupInitializationComplete() override {
    if (StartPpapiProxy(pp_instance_) == PP_TRUE) {
      NaClPluginInstance* nacl_plugin_instance =
          GetNaClPluginInstance(pp_instance_);
      JsonManifest* manifest = GetJsonManifest(pp_instance_);
      if (nacl_plugin_instance && manifest) {
        NexeLoadManager* load_manager =
            &nacl_plugin_instance->nexe_load_manager;
        std::string full_url;
        PP_PNaClOptions pnacl_options;
        JsonManifest::ErrorInfo error_info;
        if (manifest->GetProgramURL(&full_url, &pnacl_options, &error_info)) {
          int64_t exe_size = nacl_plugin_instance->pexe_size;
          if (exe_size == 0)
            exe_size = load_manager->nexe_size();
          load_manager->ReportLoadSuccess(full_url, exe_size, exe_size);
        }
      }
    }
  }

  void OpenResource(
      const std::string& key,
      ManifestServiceChannel::OpenResourceCallback callback) override {
    DCHECK(ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->
               BelongsToCurrentThread());

    // For security hardening, disable open_resource() when it is isn't
    // needed.  PNaCl pexes can't use open_resource(), but general nexes
    // and the PNaCl translator nexes may use it.
    if (process_type_ != kNativeNaClProcessType &&
        process_type_ != kPNaClTranslatorProcessType) {
      // Return an error.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::File(), 0, 0));
      return;
    }

    std::string url;
    // TODO(teravest): Clean up pnacl_options logic in JsonManifest so we don't
    // have to initialize it like this here.
    PP_PNaClOptions pnacl_options;
    pnacl_options.translate = PP_FALSE;
    pnacl_options.is_debug = PP_FALSE;
    pnacl_options.use_subzero = PP_FALSE;
    pnacl_options.opt_level = 2;
    bool is_helper_process = process_type_ == kPNaClTranslatorProcessType;
    if (!ManifestResolveKey(pp_instance_, is_helper_process, key, &url,
                            &pnacl_options)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), base::File(), 0, 0));
      return;
    }

    // We have to call DidDownloadFile, even if this object is destroyed, so
    // that the handle inside PP_NaClFileInfo isn't leaked. This means that the
    // callback passed to this function shouldn't have a weak pointer to an
    // object either.
    //
    // TODO(teravest): Make a type like PP_NaClFileInfo to use for DownloadFile
    // that would close the file handle on destruction.
    DownloadFile(pp_instance_, url,
                 base::BindOnce(&ManifestServiceProxy::DidDownloadFile,
                                std::move(callback)));
  }

 private:
  static void DidDownloadFile(
      ManifestServiceChannel::OpenResourceCallback callback,
      int32_t pp_error,
      const PP_NaClFileInfo& file_info) {
    if (pp_error != PP_OK) {
      std::move(callback).Run(base::File(), 0, 0);
      return;
    }
    std::move(callback).Run(base::File(file_info.handle), file_info.token_lo,
                            file_info.token_hi);
  }

  PP_Instance pp_instance_;
  NaClAppProcessType process_type_;
};

std::unique_ptr<blink::WebAssociatedURLLoader> CreateAssociatedURLLoader(
    const blink::WebDocument& document,
    const GURL& gurl) {
  blink::WebAssociatedURLLoaderOptions options;
  options.untrusted_http = true;
  return document.GetFrame()->CreateAssociatedURLLoader(options);
}

blink::WebURLRequest CreateWebURLRequest(const blink::WebDocument& document,
                                         const GURL& gurl) {
  blink::WebURLRequest request(gurl);
  request.SetSiteForCookies(document.SiteForCookies());

  // Follow the original behavior in the trusted plugin and
  // PepperURLLoaderHost.
  if (document.GetSecurityOrigin().CanRequest(gurl)) {
    request.SetMode(network::mojom::RequestMode::kSameOrigin);
    request.SetCredentialsMode(network::mojom::CredentialsMode::kSameOrigin);
  } else {
    request.SetMode(network::mojom::RequestMode::kCors);
    request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
  }

  // Plug-ins should not load via service workers as plug-ins may have their own
  // origin checking logic that may get confused if service workers respond with
  // resources from another origin.
  // https://w3c.github.io/ServiceWorker/#implementer-concerns
  request.SetSkipServiceWorker(true);

  return request;
}

int32_t FileDownloaderToPepperError(FileDownloader::Status status) {
  switch (status) {
    case FileDownloader::SUCCESS:
      return PP_OK;
    case FileDownloader::ACCESS_DENIED:
      return PP_ERROR_NOACCESS;
    case FileDownloader::FAILED:
      return PP_ERROR_FAILED;
    // No default case, to catch unhandled Status values.
  }
  return PP_ERROR_FAILED;
}

NaClAppProcessType PP_ToNaClAppProcessType(
    PP_NaClAppProcessType pp_process_type) {
#define STATICALLY_CHECK_NACLAPPPROCESSTYPE_EQ(pp, nonpp)        \
  static_assert(static_cast<int>(pp) == static_cast<int>(nonpp), \
                "PP_NaClAppProcessType differs from NaClAppProcessType");
  STATICALLY_CHECK_NACLAPPPROCESSTYPE_EQ(PP_UNKNOWN_NACL_PROCESS_TYPE,
                                         kUnknownNaClProcessType);
  STATICALLY_CHECK_NACLAPPPROCESSTYPE_EQ(PP_NATIVE_NACL_PROCESS_TYPE,
                                         kNativeNaClProcessType);
  STATICALLY_CHECK_NACLAPPPROCESSTYPE_EQ(PP_PNACL_PROCESS_TYPE,
                                         kPNaClProcessType);
  STATICALLY_CHECK_NACLAPPPROCESSTYPE_EQ(PP_PNACL_TRANSLATOR_PROCESS_TYPE,
                                         kPNaClTranslatorProcessType);
  STATICALLY_CHECK_NACLAPPPROCESSTYPE_EQ(PP_NUM_NACL_PROCESS_TYPES,
                                         kNumNaClProcessTypes);
#undef STATICALLY_CHECK_NACLAPPPROCESSTYPE_EQ
  DCHECK(pp_process_type > PP_UNKNOWN_NACL_PROCESS_TYPE &&
         pp_process_type < PP_NUM_NACL_PROCESS_TYPES);
  return static_cast<NaClAppProcessType>(pp_process_type);
}

}  // namespace

// Launch NaCl's sel_ldr process.
// static
void PPBNaClPrivate::LaunchSelLdr(
    PP_Instance instance,
    PP_Bool main_service_runtime,
    const char* alleged_url,
    const PP_NaClFileInfo* nexe_file_info,
    PP_NaClAppProcessType pp_process_type,
    std::unique_ptr<IPC::SyncChannel>* translator_channel,
    PP_CompletionCallback callback) {
  CHECK(ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->
            BelongsToCurrentThread());
  NaClAppProcessType process_type = PP_ToNaClAppProcessType(pp_process_type);
  // Create the manifest service proxy here, so on error case, it will be
  // destructed (without passing it to ManifestServiceChannel).
  std::unique_ptr<ManifestServiceChannel::Delegate> manifest_service_proxy(
      new ManifestServiceProxy(instance, process_type));

  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  DCHECK(plugin_instance);
  if (!load_manager || !plugin_instance) {
    if (nexe_file_info->handle != PP_kInvalidFileHandle) {
      base::File closer(nexe_file_info->handle);
    }
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE, base::BindOnce(callback.func, callback.user_data,
                                  static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }

  InstanceInfo instance_info;
  instance_info.url = GURL(alleged_url);

  // Keep backwards-compatible, but no other permissions.
  uint32_t perm_bits = ppapi::PERMISSION_DEFAULT;
  instance_info.permissions =
      ppapi::PpapiPermissions::GetForCommandLine(perm_bits);

  std::vector<NaClResourcePrefetchRequest> resource_prefetch_request_list;
  if (process_type == kNativeNaClProcessType) {
    JsonManifest* manifest = GetJsonManifest(instance);
    if (manifest) {
      manifest->GetPrefetchableFiles(&resource_prefetch_request_list);

      for (size_t i = 0; i < resource_prefetch_request_list.size(); ++i) {
        const GURL gurl(resource_prefetch_request_list[i].resource_url);
        // Important security check. Do not remove.
        if (!CanOpenViaFastPath(plugin_instance, gurl)) {
          resource_prefetch_request_list.clear();
          break;
        }
      }
    }
  }

  IPC::PlatformFileForTransit nexe_for_transit =
      IPC::InvalidPlatformFileForTransit();
#if BUILDFLAG(IS_POSIX)
  if (nexe_file_info->handle != PP_kInvalidFileHandle)
    nexe_for_transit = base::FileDescriptor(nexe_file_info->handle, true);
#else
# error Unsupported target platform.
#endif

  std::string error_message_string;
  NaClLaunchResult launch_result;
  if (!sender->Send(new NaClHostMsg_LaunchNaCl(
          NaClLaunchParams(instance_info.url.spec(), nexe_for_transit,
                           nexe_file_info->token_lo, nexe_file_info->token_hi,
                           resource_prefetch_request_list,
                           GetFrameRoutingID(instance), perm_bits,
                           process_type),
          &launch_result, &error_message_string))) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE, base::BindOnce(callback.func, callback.user_data,
                                  static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }

  if (!error_message_string.empty()) {
    // Even on error, some FDs/handles may be passed to here.
    // We must release those resources.
    // See also nacl_process_host.cc.
    if (PP_ToBool(main_service_runtime)) {
      load_manager->ReportLoadError(PP_NACL_ERROR_SEL_LDR_LAUNCH,
                                    "ServiceRuntime: failed to start",
                                    error_message_string);
    }
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE, base::BindOnce(callback.func, callback.user_data,
                                  static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }

  instance_info.channel_handle = launch_result.ppapi_ipc_channel_handle;
  instance_info.plugin_pid = launch_result.plugin_pid;
  instance_info.plugin_child_id = launch_result.plugin_child_id;

  // Don't save instance_info if channel handle is invalid.
  if (IsValidChannelHandle(instance_info.channel_handle)) {
    if (process_type == kPNaClTranslatorProcessType) {
      // Return an IPC channel which allows communicating with a PNaCl
      // translator process.
      *translator_channel = IPC::SyncChannel::Create(
          instance_info.channel_handle, IPC::Channel::MODE_CLIENT,
          /* listener = */ nullptr,
          content::RenderThread::Get()->GetIOTaskRunner(),
          base::SingleThreadTaskRunner::GetCurrentDefault(), true,
          content::RenderThread::Get()->GetShutdownEvent());
    } else {
      // Save the channel handle for when StartPpapiProxy() is called.
      NaClPluginInstance* nacl_plugin_instance =
          GetNaClPluginInstance(instance);
      nacl_plugin_instance->instance_info =
          std::make_unique<InstanceInfo>(instance_info);
    }
  }

  // Store the crash information shared memory handle.
  load_manager->set_crash_info_shmem_region(
      std::move(launch_result.crash_info_shmem_region));

  // Create the trusted plugin channel.
  if (!IsValidChannelHandle(launch_result.trusted_ipc_channel_handle)) {
    PostPPCompletionCallback(callback, PP_ERROR_FAILED);
    return;
  }
  bool is_helper_nexe = !PP_ToBool(main_service_runtime);
  std::unique_ptr<TrustedPluginChannel> trusted_plugin_channel(
      new TrustedPluginChannel(
          load_manager,
          mojo::PendingReceiver<mojom::NaClRendererHost>(
              mojo::ScopedMessagePipeHandle(
                  launch_result.trusted_ipc_channel_handle.mojo_handle)),
          is_helper_nexe));
  load_manager->set_trusted_plugin_channel(std::move(trusted_plugin_channel));

  // Create the manifest service handle as well.
  if (IsValidChannelHandle(launch_result.manifest_service_ipc_channel_handle)) {
    std::unique_ptr<ManifestServiceChannel> manifest_service_channel(
        new ManifestServiceChannel(
            launch_result.manifest_service_ipc_channel_handle,
            base::BindOnce(&PostPPCompletionCallback, callback),
            std::move(manifest_service_proxy),
            content::RenderThread::Get()->GetShutdownEvent()));
    load_manager->set_manifest_service_channel(
        std::move(manifest_service_channel));
  }
}

namespace {

PP_Bool StartPpapiProxy(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager)
    return PP_FALSE;

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance) {
    DLOG(ERROR) << "GetInstance() failed";
    return PP_FALSE;
  }

  NaClPluginInstance* nacl_plugin_instance = GetNaClPluginInstance(instance);
  if (!nacl_plugin_instance->instance_info) {
    DLOG(ERROR) << "Could not find instance ID";
    return PP_FALSE;
  }
  std::unique_ptr<InstanceInfo> instance_info =
      std::move(nacl_plugin_instance->instance_info);

  PP_ExternalPluginResult result = plugin_instance->SwitchToOutOfProcessProxy(
      base::FilePath().AppendASCII(instance_info->url.spec()),
      instance_info->permissions,
      instance_info->channel_handle,
      instance_info->plugin_pid,
      instance_info->plugin_child_id);

  if (result == PP_EXTERNAL_PLUGIN_OK) {
    // Log the amound of time that has passed between the trusted plugin being
    // initialized and the untrusted plugin being initialized.  This is
    // (roughly) the cost of using NaCl, in terms of startup time.
    load_manager->ReportStartupOverhead();
    return PP_TRUE;
  }
  if (result == PP_EXTERNAL_PLUGIN_ERROR_MODULE) {
    load_manager->ReportLoadError(PP_NACL_ERROR_START_PROXY_MODULE,
                                  "could not initialize module.");
  } else if (result == PP_EXTERNAL_PLUGIN_ERROR_INSTANCE) {
    load_manager->ReportLoadError(PP_NACL_ERROR_START_PROXY_MODULE,
                                  "could not create instance.");
  }
  return PP_FALSE;
}

// Convert a URL to a filename for GetReadonlyPnaclFd.
// Must be kept in sync with PnaclCanOpenFile() in
// components/nacl/browser/nacl_file_host.cc.
std::string PnaclComponentURLToFilename(const std::string& url) {
  // PNaCl component URLs aren't arbitrary URLs; they are always either
  // generated from ManifestResolveKey or PnaclResources::ReadResourceInfo.
  // So, it's safe to just use string parsing operations here instead of
  // URL-parsing ones.
  DCHECK(base::StartsWith(url, kPNaClTranslatorBaseUrl,
                          base::CompareCase::SENSITIVE));
  std::string r = url.substr(std::string(kPNaClTranslatorBaseUrl).length());

  // Replace characters that are not allowed with '_'.
  size_t replace_pos;
  static const char kAllowList[] = "abcdefghijklmnopqrstuvwxyz0123456789_";
  replace_pos = r.find_first_not_of(kAllowList);
  while (replace_pos != std::string::npos) {
    r = r.replace(replace_pos, 1, "_");
    replace_pos = r.find_first_not_of(kAllowList);
  }
  return r;
}

PP_FileHandle GetReadonlyPnaclFd(const std::string& url,
                                 bool is_executable,
                                 uint64_t* nonce_lo,
                                 uint64_t* nonce_hi) {
  std::string filename = PnaclComponentURLToFilename(url);
  IPC::PlatformFileForTransit out_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if (!sender->Send(new NaClHostMsg_GetReadonlyPnaclFD(
          std::string(filename), is_executable,
          &out_fd, nonce_lo, nonce_hi))) {
    return PP_kInvalidFileHandle;
  }
  if (out_fd == IPC::InvalidPlatformFileForTransit()) {
    return PP_kInvalidFileHandle;
  }
  return IPC::PlatformFileForTransitToPlatformFile(out_fd);
}

}  // namespace

// static
void PPBNaClPrivate::GetReadExecPnaclFd(const char* url,
                                        PP_NaClFileInfo* out_file_info) {
  *out_file_info = kInvalidNaClFileInfo;
  out_file_info->handle = GetReadonlyPnaclFd(url, true /* is_executable */,
                                             &out_file_info->token_lo,
                                             &out_file_info->token_hi);
}

// static
PP_FileHandle PPBNaClPrivate::CreateTemporaryFile(PP_Instance instance) {
  IPC::PlatformFileForTransit transit_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if (!sender->Send(new NaClHostMsg_NaClCreateTemporaryFile(
          &transit_fd))) {
    return PP_kInvalidFileHandle;
  }

  if (transit_fd == IPC::InvalidPlatformFileForTransit()) {
    return PP_kInvalidFileHandle;
  }

  return IPC::PlatformFileForTransitToPlatformFile(transit_fd);
}

// static
int32_t PPBNaClPrivate::GetNumberOfProcessors() {
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  int32_t num_processors = 1;
  return sender->Send(new NaClHostMsg_NaClGetNumProcessors(&num_processors)) ?
      num_processors : 1;
}

namespace {

void GetNexeFd(PP_Instance instance,
               const std::string& pexe_url,
               uint32_t opt_level,
               const base::Time& last_modified_time,
               const std::string& etag,
               bool has_no_store_header,
               bool use_subzero,
               PnaclTranslationResourceHost::RequestNexeFdCallback callback) {
  if (!InitializePnaclResourceHost()) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  static_cast<int32_t>(PP_ERROR_FAILED), false,
                                  PP_kInvalidFileHandle));
    return;
  }

  PnaclCacheInfo cache_info;
  cache_info.pexe_url = GURL(pexe_url);
  // TODO(dschuff): Get this value from the pnacl json file after it
  // rolls in from NaCl.
  cache_info.abi_version = 1;
  cache_info.opt_level = opt_level;
  cache_info.last_modified = last_modified_time;
  cache_info.etag = etag;
  cache_info.has_no_store_header = has_no_store_header;
  cache_info.use_subzero = use_subzero;
  cache_info.sandbox_isa = GetSandboxArch();
  cache_info.extra_flags = GetCpuFeatures();

  g_pnacl_resource_host.Get()->RequestNexeFd(instance, cache_info,
                                             std::move(callback));
}

void LogTranslationFinishedUMA(const std::string& uma_suffix,
                               int32_t opt_level,
                               int32_t unknown_opt_level,
                               int64_t nexe_size,
                               int64_t pexe_size,
                               int64_t compile_time_us,
                               base::TimeDelta total_time) {
  HistogramEnumerate("NaCl.Options.PNaCl.OptLevel" + uma_suffix, opt_level,
                     unknown_opt_level + 1);
  HistogramKBPerSec("NaCl.Perf.PNaClLoadTime.CompileKBPerSec" + uma_suffix,
                    pexe_size / 1024, compile_time_us);
  HistogramSizeKB("NaCl.Perf.Size.PNaClTranslatedNexe" + uma_suffix,
                  nexe_size / 1024);
  HistogramSizeKB("NaCl.Perf.Size.Pexe" + uma_suffix, pexe_size / 1024);
  HistogramRatio("NaCl.Perf.Size.PexeNexeSizePct" + uma_suffix, pexe_size,
                 nexe_size);
  HistogramTimeTranslation(
      "NaCl.Perf.PNaClLoadTime.TotalUncachedTime" + uma_suffix,
      total_time.InMilliseconds());
  HistogramKBPerSec(
      "NaCl.Perf.PNaClLoadTime.TotalUncachedKBPerSec" + uma_suffix,
      pexe_size / 1024, total_time.InMicroseconds());
}

}  // namespace

// static
void PPBNaClPrivate::ReportTranslationFinished(PP_Instance instance,
                                               PP_Bool success,
                                               int32_t opt_level,
                                               PP_Bool use_subzero,
                                               int64_t nexe_size,
                                               int64_t pexe_size,
                                               int64_t compile_time_us) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (success == PP_TRUE && load_manager) {
    base::TimeDelta total_time =
        base::Time::Now() - load_manager->pnacl_start_time();
    static const int32_t kUnknownOptLevel = 4;
    if (opt_level < 0 || opt_level > 3)
      opt_level = kUnknownOptLevel;
    // Log twice: once to cover all PNaCl UMA, and then a second
    // time with the more specific UMA (Subzero vs LLC).
    std::string uma_suffix(use_subzero ? ".Subzero" : ".LLC");
    LogTranslationFinishedUMA("", opt_level, kUnknownOptLevel, nexe_size,
                              pexe_size, compile_time_us, total_time);
    LogTranslationFinishedUMA(uma_suffix, opt_level, kUnknownOptLevel,
                              nexe_size, pexe_size, compile_time_us,
                              total_time);
  }

  // If the resource host isn't initialized, don't try to do that here.
  // Just return because something is already very wrong.
  if (g_pnacl_resource_host.Get().get() == NULL)
    return;
  g_pnacl_resource_host.Get()->ReportTranslationFinished(instance, success);

  // Record the pexe size for reporting in a later load event.
  NaClPluginInstance* nacl_plugin_instance = GetNaClPluginInstance(instance);
  if (nacl_plugin_instance) {
    nacl_plugin_instance->pexe_size = pexe_size;
  }
}

namespace {

PP_FileHandle OpenNaClExecutable(PP_Instance instance,
                                 const char* file_url,
                                 uint64_t* nonce_lo,
                                 uint64_t* nonce_hi) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager)
    return PP_kInvalidFileHandle;

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance)
    return PP_kInvalidFileHandle;

  GURL gurl(file_url);
  // Important security check. Do not remove.
  if (!CanOpenViaFastPath(plugin_instance, gurl))
    return PP_kInvalidFileHandle;

  IPC::PlatformFileForTransit out_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  *nonce_lo = 0;
  *nonce_hi = 0;
  base::FilePath file_path;
  if (!sender->Send(new NaClHostMsg_OpenNaClExecutable(
          GetFrameRoutingID(instance), GURL(file_url), &out_fd, nonce_lo,
          nonce_hi))) {
    return PP_kInvalidFileHandle;
  }

  if (out_fd == IPC::InvalidPlatformFileForTransit())
    return PP_kInvalidFileHandle;

  return IPC::PlatformFileForTransitToPlatformFile(out_fd);
}

}  // namespace

// static
void PPBNaClPrivate::DispatchEvent(PP_Instance instance,
                                   PP_NaClEventType event_type,
                                   const char* resource_url,
                                   PP_Bool length_is_computable,
                                   uint64_t loaded_bytes,
                                   uint64_t total_bytes) {
  ProgressEvent event(event_type,
                      resource_url,
                      PP_ToBool(length_is_computable),
                      loaded_bytes,
                      total_bytes);
  DispatchProgressEvent(instance, event);
}

// static
void PPBNaClPrivate::ReportLoadError(PP_Instance instance,
                                     PP_NaClError error,
                                     const char* error_message) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->ReportLoadError(error, error_message);
}

// static
void PPBNaClPrivate::InstanceCreated(PP_Instance instance) {
  InstanceMap& map = g_instance_map.Get();
  CHECK(map.find(instance) == map.end());  // Sanity check.
  std::unique_ptr<NaClPluginInstance> new_instance(
      new NaClPluginInstance(instance));
  map[instance] = std::move(new_instance);
}

// static
void PPBNaClPrivate::InstanceDestroyed(PP_Instance instance) {
  InstanceMap& map = g_instance_map.Get();
  auto iter = map.find(instance);
  CHECK(iter != map.end());
  // The erase may call NexeLoadManager's destructor prior to removing it from
  // the map. In that case, it is possible for the trusted Plugin to re-enter
  // the NexeLoadManager (e.g., by calling ReportLoadError). Passing out the
  // NexeLoadManager to a local scoped_ptr just ensures that its entry is gone
  // from the map prior to the destructor being invoked.
  std::unique_ptr<NaClPluginInstance> temp = std::move(iter->second);
  map.erase(iter);
}

// static
void PPBNaClPrivate::TerminateNaClLoader(PP_Instance instance) {
  auto* load_mgr = GetNexeLoadManager(instance);
  if (load_mgr)
    load_mgr->CloseTrustedPluginChannel();
}

namespace {

PP_Bool NaClDebugEnabledForURL(const char* alleged_nmf_url) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNaClDebug))
    return PP_FALSE;
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  bool should_debug = false;
  return PP_FromBool(
      sender->Send(new NaClHostMsg_NaClDebugEnabledForURL(GURL(alleged_nmf_url),
                                                          &should_debug)) &&
      should_debug);
}

}  // namespace

// static
void PPBNaClPrivate::InitializePlugin(PP_Instance instance,
                                      uint32_t argc,
                                      const char* argn[],
                                      const char* argv[]) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (load_manager)
    load_manager->InitializePlugin(argc, argn, argv);
}

namespace {

void DownloadManifestToBuffer(PP_Instance instance,
                              struct PP_CompletionCallback callback);

bool CreateJsonManifest(PP_Instance instance,
                        const std::string& manifest_url,
                        const std::string& manifest_data);

}  // namespace

// static
void PPBNaClPrivate::RequestNaClManifest(PP_Instance instance,
                                         PP_CompletionCallback callback) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE, base::BindOnce(callback.func, callback.user_data,
                                  static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }

  std::string url = load_manager->GetManifestURLArgument();
  if (url.empty() || !load_manager->RequestNaClManifest(url)) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE, base::BindOnce(callback.func, callback.user_data,
                                  static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }

  const GURL& base_url = load_manager->manifest_base_url();
  if (base_url.SchemeIs("data")) {
    GURL gurl(base_url);
    std::string mime_type;
    std::string charset;
    std::string data;
    int32_t error = PP_ERROR_FAILED;
    if (net::DataURL::Parse(gurl, &mime_type, &charset, &data)) {
      if (data.size() <= ManifestDownloader::kNaClManifestMaxFileBytes) {
        if (CreateJsonManifest(instance, base_url.spec(), data))
          error = PP_OK;
      } else {
        load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_TOO_LARGE,
                                      "manifest file too large.");
      }
    } else {
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_LOAD_URL,
                                    "could not load manifest url.");
    }
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE, base::BindOnce(callback.func, callback.user_data, error));
  } else {
    DownloadManifestToBuffer(instance, callback);
  }
}

// static
PP_Var PPBNaClPrivate::GetManifestBaseURL(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager)
    return PP_MakeUndefined();
  const GURL& gurl = load_manager->manifest_base_url();
  if (!gurl.is_valid())
    return PP_MakeUndefined();
  return ppapi::StringVar::StringToPPVar(gurl.spec());
}

// static
void PPBNaClPrivate::ProcessNaClManifest(PP_Instance instance,
                                         const char* program_url) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->ProcessNaClManifest(program_url);
}

namespace {

void DownloadManifestToBufferCompletion(PP_Instance instance,
                                        struct PP_CompletionCallback callback,
                                        base::Time start_time,
                                        PP_NaClError pp_nacl_error,
                                        const std::string& data);

void DownloadManifestToBuffer(PP_Instance instance,
                              struct PP_CompletionCallback callback) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!load_manager || !plugin_instance) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE, base::BindOnce(callback.func, callback.user_data,
                                  static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }
  const blink::WebDocument& document =
      plugin_instance->GetContainer()->GetDocument();

  const GURL& gurl = load_manager->manifest_base_url();
  std::unique_ptr<blink::WebAssociatedURLLoader> url_loader(
      CreateAssociatedURLLoader(document, gurl));
  blink::WebURLRequest request = CreateWebURLRequest(document, gurl);

  // Requests from plug-ins must skip service workers, see the comment in
  // CreateWebURLRequest.
  DCHECK(request.GetSkipServiceWorker());

  // ManifestDownloader deletes itself after invoking the callback.
  ManifestDownloader* manifest_downloader = new ManifestDownloader(
      std::move(url_loader), load_manager->is_installed(),
      base::BindOnce(DownloadManifestToBufferCompletion, instance, callback,
                     base::Time::Now()));
  manifest_downloader->Load(request);
}

void DownloadManifestToBufferCompletion(PP_Instance instance,
                                        struct PP_CompletionCallback callback,
                                        base::Time start_time,
                                        PP_NaClError pp_nacl_error,
                                        const std::string& data) {
  base::TimeDelta download_time = base::Time::Now() - start_time;
  HistogramTimeSmall("NaCl.Perf.StartupTime.ManifestDownload",
                     download_time.InMilliseconds());

  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (!load_manager) {
    callback.func(callback.user_data, PP_ERROR_ABORTED);
    return;
  }

  int32_t pp_error;
  switch (pp_nacl_error) {
    case PP_NACL_ERROR_LOAD_SUCCESS:
      pp_error = PP_OK;
      break;
    case PP_NACL_ERROR_MANIFEST_LOAD_URL:
      pp_error = PP_ERROR_FAILED;
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_LOAD_URL,
                                    "could not load manifest url.");
      break;
    case PP_NACL_ERROR_MANIFEST_TOO_LARGE:
      pp_error = PP_ERROR_FILETOOBIG;
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_TOO_LARGE,
                                    "manifest file too large.");
      break;
    case PP_NACL_ERROR_MANIFEST_NOACCESS_URL:
      pp_error = PP_ERROR_NOACCESS;
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_NOACCESS_URL,
                                    "access to manifest url was denied.");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      pp_error = PP_ERROR_FAILED;
      load_manager->ReportLoadError(PP_NACL_ERROR_MANIFEST_LOAD_URL,
                                    "could not load manifest url.");
  }

  if (pp_error == PP_OK) {
    std::string base_url = load_manager->manifest_base_url().spec();
    if (!CreateJsonManifest(instance, base_url, data))
      pp_error = PP_ERROR_FAILED;
  }
  callback.func(callback.user_data, pp_error);
}

bool CreateJsonManifest(PP_Instance instance,
                        const std::string& manifest_url,
                        const std::string& manifest_data) {
  HistogramSizeKB("NaCl.Perf.Size.Manifest",
                  static_cast<int32_t>(manifest_data.length() / 1024));

  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (!load_manager)
    return false;

  const char* isa_type;
  if (load_manager->IsPNaCl())
    isa_type = kPortableArch;
  else
    isa_type = GetSandboxArch();

  std::unique_ptr<nacl::JsonManifest> j(new nacl::JsonManifest(
      manifest_url.c_str(), isa_type,
      PP_ToBool(NaClDebugEnabledForURL(manifest_url.c_str()))));
  JsonManifest::ErrorInfo error_info;
  if (j->Init(manifest_data.c_str(), &error_info)) {
    GetNaClPluginInstance(instance)->json_manifest = std::move(j);
    return true;
  }
  load_manager->ReportLoadError(error_info.error, error_info.string);
  return false;
}

bool ShouldUseSubzero(const PP_PNaClOptions* pnacl_options) {
  // Always use Subzero if explicitly overridden on the command line.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForcePNaClSubzero))
    return true;
  // Otherwise, don't use Subzero for a debug pexe file since Subzero's parser
  // is likely to reject an unfinalized pexe.
  if (pnacl_options->is_debug)
    return false;
  // Only use Subzero for optlevel=0.
  if (pnacl_options->opt_level != 0)
    return false;
  // Check a list of allowed architectures.
  const char* arch = GetSandboxArch();
  if (strcmp(arch, "x86-32") == 0)
    return true;
  if (strcmp(arch, "x86-64") == 0)
    return true;
  if (strcmp(arch, "arm") == 0)
    return true;

  return false;
}

}  // namespace

// static
PP_Bool PPBNaClPrivate::GetManifestProgramURL(PP_Instance instance,
                                              PP_Var* pp_full_url,
                                              PP_PNaClOptions* pnacl_options) {
  nacl::NexeLoadManager* load_manager = GetNexeLoadManager(instance);

  JsonManifest* manifest = GetJsonManifest(instance);
  if (manifest == NULL)
    return PP_FALSE;

  std::string full_url;
  JsonManifest::ErrorInfo error_info;
  if (manifest->GetProgramURL(&full_url, pnacl_options, &error_info)) {
    *pp_full_url = ppapi::StringVar::StringToPPVar(full_url);
    if (ShouldUseSubzero(pnacl_options)) {
      pnacl_options->use_subzero = PP_TRUE;
      // Subzero -O2 is closer to LLC -O0, so indicate -O2.
      pnacl_options->opt_level = 2;
    }
    return PP_TRUE;
  }

  if (load_manager)
    load_manager->ReportLoadError(error_info.error, error_info.string);
  return PP_FALSE;
}

namespace {

bool ManifestResolveKey(PP_Instance instance,
                        bool is_helper_process,
                        const std::string& key,
                        std::string* full_url,
                        PP_PNaClOptions* pnacl_options) {
  // For "helper" processes (llc and ld, for PNaCl translation), we resolve
  // keys manually as there is no existing .nmf file to parse.
  if (is_helper_process) {
    pnacl_options->translate = PP_FALSE;
    *full_url = std::string(kPNaClTranslatorBaseUrl) + GetSandboxArch() + "/" +
                key;
    return true;
  }

  JsonManifest* manifest = GetJsonManifest(instance);
  if (manifest == NULL)
    return false;

  return manifest->ResolveKey(key, full_url, pnacl_options);
}

}  // namespace

// static
PP_Bool PPBNaClPrivate::GetPnaclResourceInfo(PP_Instance instance,
                                             PP_Var* llc_tool_name,
                                             PP_Var* ld_tool_name,
                                             PP_Var* subzero_tool_name) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  CHECK(load_manager);

  const auto get_info = [&]() -> base::expected<void, std::string> {
    const std::string kFilename = "chrome://pnacl-translator/pnacl.json";
    uint64_t nonce_lo = 0;
    uint64_t nonce_hi = 0;
    base::File file(GetReadonlyPnaclFd(kFilename, false /* is_executable */,
                                       &nonce_lo, &nonce_hi));
    if (!file.IsValid()) {
      return base::unexpected(
          "The Portable Native Client (pnacl) component is not installed. "
          "Please consult chrome://components for more information.");
    }

    int64_t file_size = file.GetLength();
    if (file_size < 0) {
      return base::unexpected("GetPnaclResourceInfo, GetLength failed for: " +
                              kFilename);
    }

    if (file_size > 1 << 20) {
      return base::unexpected("GetPnaclResourceInfo, file too large: " +
                              kFilename);
    }

    auto buffer = base::HeapArray<char>::Uninit(file_size + 1);
    int rc = UNSAFE_TODO(file.Read(0, buffer.data(), file_size));
    if (rc < 0 || rc != file_size) {
      return base::unexpected("GetPnaclResourceInfo, reading failed for: " +
                              kFilename);
    }

    // Null-terminate the bytes we we read from the file.
    buffer[rc] = 0;

    // Expect the JSON file to contain a top-level object (dictionary).
    ASSIGN_OR_RETURN(
        auto parsed_json,
        base::JSONReader::ReadAndReturnValueWithError(buffer.data()),
        [](base::JSONReader::Error error) {
          return "Parsing resource info failed: JSON parse error: " +
                 std::move(error).message;
        });

    auto* json_dict = parsed_json.GetIfDict();
    if (!json_dict) {
      return base::unexpected(
          "Parsing resource info failed: JSON parse error: Not a "
          "dictionary.");
    }

    if (auto* pnacl_llc_name = json_dict->FindString("pnacl-llc-name")) {
      *llc_tool_name = ppapi::StringVar::StringToPPVar(*pnacl_llc_name);
    }
    if (auto* pnacl_ld_name = json_dict->FindString("pnacl-ld-name")) {
      *ld_tool_name = ppapi::StringVar::StringToPPVar(*pnacl_ld_name);
    }
    if (auto* pnacl_sz_name = json_dict->FindString("pnacl-sz-name")) {
      *subzero_tool_name = ppapi::StringVar::StringToPPVar(*pnacl_sz_name);
    }
    return base::ok();
  };
  RETURN_IF_ERROR(get_info(), [&](std::string error) {
    load_manager->ReportLoadError(PP_NACL_ERROR_PNACL_RESOURCE_FETCH, error);
    return PP_FALSE;
  });
  return PP_TRUE;
}

// static
const char* PPBNaClPrivate::GetSandboxArch() {
  return nacl::GetSandboxArch();
}

// static
PP_Var PPBNaClPrivate::GetCpuFeatureAttrs() {
  return ppapi::StringVar::StringToPPVar(GetCpuFeatures());
}

namespace {

// Encapsulates some of the state for a call to DownloadNexe to prevent
// argument lists from getting too long.
struct DownloadNexeRequest {
  PP_Instance instance;
  std::string url;
  PP_CompletionCallback callback;
  base::Time start_time;
};

// A utility class to ensure that we don't send progress events more often than
// every 10ms for a given file.
class ProgressEventRateLimiter {
 public:
  explicit ProgressEventRateLimiter(PP_Instance instance)
      : instance_(instance) { }

  void ReportProgress(const std::string& url,
                      int64_t total_bytes_received,
                      int64_t total_bytes_to_be_received) {
    base::Time now = base::Time::Now();
    if (now - last_event_ > base::Milliseconds(10)) {
      DispatchProgressEvent(instance_,
                            ProgressEvent(PP_NACL_EVENT_PROGRESS,
                                          url,
                                          total_bytes_to_be_received >= 0,
                                          total_bytes_received,
                                          total_bytes_to_be_received));
      last_event_ = now;
    }
  }

 private:
  PP_Instance instance_;
  base::Time last_event_;
};

void DownloadNexeCompletion(const DownloadNexeRequest& request,
                            PP_NaClFileInfo* out_file_info,
                            FileDownloader::Status status,
                            base::File target_file,
                            int http_status);

}  // namespace

// static
void PPBNaClPrivate::DownloadNexe(PP_Instance instance,
                                  const char* url,
                                  PP_NaClFileInfo* out_file_info,
                                  PP_CompletionCallback callback) {
  CHECK(url);
  CHECK(out_file_info);
  DownloadNexeRequest request;
  request.instance = instance;
  request.url = url;
  request.callback = callback;
  request.start_time = base::Time::Now();

  // Try the fast path for retrieving the file first.
  PP_FileHandle handle = OpenNaClExecutable(instance,
                                            url,
                                            &out_file_info->token_lo,
                                            &out_file_info->token_hi);
  if (handle != PP_kInvalidFileHandle) {
    DownloadNexeCompletion(request,
                           out_file_info,
                           FileDownloader::SUCCESS,
                           base::File(handle),
                           200);
    return;
  }

  // The fast path didn't work, we'll fetch the file using URLLoader and write
  // it to local storage.
  base::File target_file(PPBNaClPrivate::CreateTemporaryFile(instance));
  GURL gurl(url);

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance) {
    ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
        FROM_HERE, base::BindOnce(callback.func, callback.user_data,
                                  static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }
  const blink::WebDocument& document =
      plugin_instance->GetContainer()->GetDocument();
  std::unique_ptr<blink::WebAssociatedURLLoader> url_loader(
      CreateAssociatedURLLoader(document, gurl));
  blink::WebURLRequest url_request = CreateWebURLRequest(document, gurl);

  ProgressEventRateLimiter* tracker = new ProgressEventRateLimiter(instance);

  // FileDownloader deletes itself after invoking DownloadNexeCompletion.
  FileDownloader* file_downloader = new FileDownloader(
      std::move(url_loader), std::move(target_file),
      base::BindOnce(&DownloadNexeCompletion, request, out_file_info),
      base::BindRepeating(&ProgressEventRateLimiter::ReportProgress,
                          base::Owned(tracker), std::string(url)));
  file_downloader->Load(url_request);
}

namespace {

void DownloadNexeCompletion(const DownloadNexeRequest& request,
                            PP_NaClFileInfo* out_file_info,
                            FileDownloader::Status status,
                            base::File target_file,
                            int http_status) {
  int32_t pp_error = FileDownloaderToPepperError(status);
  int64_t bytes_read = -1;
  if (pp_error == PP_OK && target_file.IsValid()) {
    base::File::Info info;
    if (target_file.GetInfo(&info))
      bytes_read = info.size;
  }

  if (bytes_read == -1) {
    target_file.Close();
    pp_error = PP_ERROR_FAILED;
  }

  base::TimeDelta download_time = base::Time::Now() - request.start_time;

  NexeLoadManager* load_manager = GetNexeLoadManager(request.instance);
  if (load_manager) {
    load_manager->NexeFileDidOpen(pp_error,
                                  target_file,
                                  http_status,
                                  bytes_read,
                                  request.url,
                                  download_time);
  }

  if (pp_error == PP_OK && target_file.IsValid())
    out_file_info->handle = target_file.TakePlatformFile();
  else
    out_file_info->handle = PP_kInvalidFileHandle;

  request.callback.func(request.callback.user_data, pp_error);
}

void DownloadFileCompletion(DownloadFileCallback callback,
                            FileDownloader::Status status,
                            base::File file,
                            int http_status) {
  int32_t pp_error = FileDownloaderToPepperError(status);
  PP_NaClFileInfo file_info;
  if (pp_error == PP_OK) {
    file_info.handle = file.TakePlatformFile();
    file_info.token_lo = 0;
    file_info.token_hi = 0;
  } else {
    file_info = kInvalidNaClFileInfo;
  }

  std::move(callback).Run(pp_error, file_info);
}

void DownloadFile(PP_Instance instance,
                  const std::string& url,
                  DownloadFileCallback callback) {
  DCHECK(ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->
             BelongsToCurrentThread());

  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  DCHECK(load_manager);
  if (!load_manager) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  static_cast<int32_t>(PP_ERROR_FAILED),
                                  kInvalidNaClFileInfo));
    return;
  }

  // Handle special PNaCl support files which are installed on the user's
  // machine.
  if (base::StartsWith(url, kPNaClTranslatorBaseUrl,
                       base::CompareCase::SENSITIVE)) {
    PP_NaClFileInfo file_info = kInvalidNaClFileInfo;
    PP_FileHandle handle =
        GetReadonlyPnaclFd(url, false /* is_executable */, &file_info.token_lo,
                           &file_info.token_hi);
    if (handle == PP_kInvalidFileHandle) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    static_cast<int32_t>(PP_ERROR_FAILED),
                                    kInvalidNaClFileInfo));
      return;
    }
    file_info.handle = handle;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  static_cast<int32_t>(PP_OK), file_info));
    return;
  }

  // We have to ensure that this url resolves relative to the plugin base url
  // before downloading it.
  const GURL& test_gurl = load_manager->plugin_base_url().Resolve(url);
  if (!test_gurl.is_valid()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  static_cast<int32_t>(PP_ERROR_FAILED),
                                  kInvalidNaClFileInfo));
    return;
  }

  // Try the fast path for retrieving the file first.
  uint64_t file_token_lo = 0;
  uint64_t file_token_hi = 0;
  PP_FileHandle file_handle = OpenNaClExecutable(instance,
                                                 url.c_str(),
                                                 &file_token_lo,
                                                 &file_token_hi);
  if (file_handle != PP_kInvalidFileHandle) {
    PP_NaClFileInfo file_info;
    file_info.handle = file_handle;
    file_info.token_lo = file_token_lo;
    file_info.token_hi = file_token_hi;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  static_cast<int32_t>(PP_OK), file_info));
    return;
  }

  // The fast path didn't work, we'll fetch the file using URLLoader and write
  // it to local storage.
  base::File target_file(PPBNaClPrivate::CreateTemporaryFile(instance));
  GURL gurl(url);

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  static_cast<int32_t>(PP_ERROR_FAILED),
                                  kInvalidNaClFileInfo));
    return;
  }
  const blink::WebDocument& document =
      plugin_instance->GetContainer()->GetDocument();
  std::unique_ptr<blink::WebAssociatedURLLoader> url_loader(
      CreateAssociatedURLLoader(document, gurl));
  blink::WebURLRequest url_request = CreateWebURLRequest(document, gurl);

  ProgressEventRateLimiter* tracker = new ProgressEventRateLimiter(instance);

  // FileDownloader deletes itself after invoking DownloadNexeCompletion.
  FileDownloader* file_downloader = new FileDownloader(
      std::move(url_loader), std::move(target_file),
      base::BindOnce(&DownloadFileCompletion, std::move(callback)),
      base::BindRepeating(&ProgressEventRateLimiter::ReportProgress,
                          base::Owned(tracker), std::string(url)));
  file_downloader->Load(url_request);
}

}  // namespace

// static
void PPBNaClPrivate::LogTranslateTime(const char* histogram_name,
                                      int64_t time_in_us) {
  ppapi::PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostTask(
      FROM_HERE,
      base::BindOnce(&HistogramTimeTranslation, std::string(histogram_name),
                     time_in_us / 1000));
}

// static
void PPBNaClPrivate::LogBytesCompiledVsDownloaded(
    PP_Bool use_subzero,
    int64_t pexe_bytes_compiled,
    int64_t pexe_bytes_downloaded) {
  HistogramRatio("NaCl.Perf.PNaClLoadTime.PctCompiledWhenFullyDownloaded",
                 pexe_bytes_compiled, pexe_bytes_downloaded);
  HistogramRatio(
      use_subzero
          ? "NaCl.Perf.PNaClLoadTime.PctCompiledWhenFullyDownloaded.Subzero"
          : "NaCl.Perf.PNaClLoadTime.PctCompiledWhenFullyDownloaded.LLC",
      pexe_bytes_compiled, pexe_bytes_downloaded);
}

// static
void PPBNaClPrivate::SetPNaClStartTime(PP_Instance instance) {
  NexeLoadManager* load_manager = GetNexeLoadManager(instance);
  if (load_manager)
    load_manager->set_pnacl_start_time(base::Time::Now());
}

namespace {

// PexeDownloader is responsible for deleting itself when the download
// finishes.
class PexeDownloader : public blink::WebAssociatedURLLoaderClient {
 public:
  PexeDownloader(PP_Instance instance,
                 std::unique_ptr<blink::WebAssociatedURLLoader> url_loader,
                 const std::string& pexe_url,
                 int32_t pexe_opt_level,
                 bool use_subzero,
                 const PPP_PexeStreamHandler* stream_handler,
                 void* stream_handler_user_data)
      : instance_(instance),
        url_loader_(std::move(url_loader)),
        pexe_url_(pexe_url),
        pexe_opt_level_(pexe_opt_level),
        use_subzero_(use_subzero),
        stream_handler_(stream_handler),
        stream_handler_user_data_(stream_handler_user_data),
        success_(false),
        expected_content_length_(-1) {}

  void Load(const blink::WebURLRequest& request) {
    url_loader_->LoadAsynchronously(request, this);
  }

 private:
  void DidReceiveResponse(const blink::WebURLResponse& response) override {
    success_ = (response.HttpStatusCode() == 200);
    if (!success_)
      return;

    expected_content_length_ = response.ExpectedContentLength();

    // Defer loading after receiving headers. This is because we may already
    // have a cached translated nexe, so check for that now.
    url_loader_->SetDefersLoading(true);

    std::string etag = response.HttpHeaderField("etag").Utf8();

    // Parse the "last-modified" date string. An invalid string will result
    // in a base::Time value of 0, which is supported by the only user of
    // the |CacheInfo::last_modified| field (see
    // pnacl::PnaclTranslationCache::GetKey()).
    std::string last_modified =
        response.HttpHeaderField("last-modified").Utf8();
    base::Time last_modified_time;
    std::ignore =
        base::Time::FromString(last_modified.c_str(), &last_modified_time);

    bool has_no_store_header = false;
    std::string cache_control =
        response.HttpHeaderField("cache-control").Utf8();

    for (const std::string& cur : base::SplitString(
             cache_control, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      if (base::ToLowerASCII(cur) == "no-store")
        has_no_store_header = true;
    }

    GetNexeFd(instance_, pexe_url_, pexe_opt_level_, last_modified_time, etag,
              has_no_store_header, use_subzero_,
              base::BindOnce(&PexeDownloader::didGetNexeFd,
                             weak_factory_.GetWeakPtr()));
  }

  void didGetNexeFd(int32_t pp_error,
                    bool cache_hit,
                    PP_FileHandle file_handle) {
    if (!content::PepperPluginInstance::Get(instance_)) {
      delete this;
      return;
    }

    HistogramEnumerate("NaCl.Perf.PNaClCache.IsHit", cache_hit, 2);
    HistogramEnumerate(use_subzero_ ? "NaCl.Perf.PNaClCache.IsHit.Subzero"
                                    : "NaCl.Perf.PNaClCache.IsHit.LLC",
                       cache_hit, 2);
    if (cache_hit) {
      stream_handler_->DidCacheHit(stream_handler_user_data_, file_handle);

      // We delete the PexeDownloader at this point since we successfully got a
      // cached, translated nexe.
      delete this;
      return;
    }
    stream_handler_->DidCacheMiss(stream_handler_user_data_,
                                  expected_content_length_,
                                  file_handle);

    // No translated nexe was found in the cache, so we should download the
    // file to start streaming it.
    url_loader_->SetDefersLoading(false);
  }

  void DidReceiveData(base::span<const char> data) override {
    if (content::PepperPluginInstance::Get(instance_)) {
      // Stream the data we received to the stream callback.
      stream_handler_->DidStreamData(stream_handler_user_data_, data.data(),
                                     base::checked_cast<int32_t>(data.size()));
    }
  }

  void DidFinishLoading() override {
    int32_t result = success_ ? PP_OK : PP_ERROR_FAILED;

    if (content::PepperPluginInstance::Get(instance_))
      stream_handler_->DidFinishStream(stream_handler_user_data_, result);
    delete this;
  }

  void DidFail(const blink::WebURLError& error) override {
    if (content::PepperPluginInstance::Get(instance_))
      stream_handler_->DidFinishStream(stream_handler_user_data_,
                                       PP_ERROR_FAILED);
    delete this;
  }

  PP_Instance instance_;
  std::unique_ptr<blink::WebAssociatedURLLoader> url_loader_;
  std::string pexe_url_;
  int32_t pexe_opt_level_;
  bool use_subzero_;
  raw_ptr<const PPP_PexeStreamHandler> stream_handler_;
  raw_ptr<void> stream_handler_user_data_;
  bool success_;
  int64_t expected_content_length_;
  base::WeakPtrFactory<PexeDownloader> weak_factory_{this};
};

}  // namespace

// static
void PPBNaClPrivate::StreamPexe(PP_Instance instance,
                                const char* pexe_url,
                                int32_t opt_level,
                                PP_Bool use_subzero,
                                const PPP_PexeStreamHandler* handler,
                                void* handler_user_data) {
  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(handler->DidFinishStream, handler_user_data,
                                  static_cast<int32_t>(PP_ERROR_FAILED)));
    return;
  }

  GURL gurl(pexe_url);
  const blink::WebDocument& document =
      plugin_instance->GetContainer()->GetDocument();
  std::unique_ptr<blink::WebAssociatedURLLoader> url_loader(
      CreateAssociatedURLLoader(document, gurl));
  PexeDownloader* downloader =
      new PexeDownloader(instance, std::move(url_loader), pexe_url, opt_level,
                         PP_ToBool(use_subzero), handler, handler_user_data);

  blink::WebURLRequest url_request = CreateWebURLRequest(document, gurl);
  // Mark the request as requesting a PNaCl bitcode file,
  // so that component updater can detect this user action.
  url_request.AddHttpHeaderField(
      blink::WebString::FromUTF8("Accept"),
      blink::WebString::FromUTF8("application/x-pnacl, */*"));
  url_request.SetRequestContext(blink::mojom::RequestContextType::OBJECT);
  downloader->Load(url_request);
}

}  // namespace nacl
