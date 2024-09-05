// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/metrics/cast_metrics_service_client.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/mojom/interface_provider.mojom-forward.h"
#include "services/service_manager/public/mojom/service.mojom-forward.h"
#include "storage/browser/quota/quota_settings.h"

class PrefService;

namespace base {
struct OnTaskRunnerDeleter;
}

namespace breakpad {
class CrashHandlerHostLinux;
}

namespace device {
class BluetoothAdapterCast;
}

namespace media {
class CdmFactory;
}

namespace metrics {
class MetricsService;
}

namespace net {
class SSLPrivateKey;
class X509Certificate;
}

namespace chromecast {
class CastService;
class CastSystemMemoryPressureEvaluatorAdjuster;
class CastWebService;
class CastWindowManager;
class CastFeatureListCreator;
class DisplaySettingsManager;
class GeneralAudienceBrowsingService;
class MemoryPressureControllerImpl;
class ServiceConnector;

namespace media {
class MediaCapsImpl;
class CmaBackendFactory;
class MediaPipelineBackendManager;
class MediaResourceTracker;
class VideoGeometrySetterService;
class VideoPlaneController;
class VideoModeSwitcher;
class VideoResolutionPolicy;
}

namespace shell {
class CastBrowserMainParts;
class CastNetworkContexts;

class CastContentBrowserClient
    : public content::ContentBrowserClient,
      public chromecast::metrics::CastMetricsServiceDelegate {
 public:
  // Creates an implementation of CastContentBrowserClient. Platform should
  // link in an implementation as needed.
  static std::unique_ptr<CastContentBrowserClient> Create(
      CastFeatureListCreator* cast_feature_list_creator);

  // Returns a list of headers that will be exempt from CORS preflight checks.
  // This is needed since currently servers don't have the correct response to
  // preflight checks.
  static std::vector<std::string> GetCorsExemptHeadersList();

  CastContentBrowserClient(const CastContentBrowserClient&) = delete;
  CastContentBrowserClient& operator=(const CastContentBrowserClient&) = delete;

  ~CastContentBrowserClient() override;

  // Creates a ServiceConnector for routing Cast-related service interface
  // binding requests.
  virtual std::unique_ptr<chromecast::ServiceConnector>
  CreateServiceConnector();

  // Creates and returns the CastService instance for the current process.
  virtual std::unique_ptr<CastService> CreateCastService(
      content::BrowserContext* browser_context,
      CastSystemMemoryPressureEvaluatorAdjuster*
          cast_system_memory_pressure_evaluator_adjuster,
      PrefService* pref_service,
      media::VideoPlaneController* video_plane_controller,
      CastWindowManager* window_manager,
      CastWebService* web_service,
      DisplaySettingsManager* display_settings_manager);

  virtual media::VideoModeSwitcher* GetVideoModeSwitcher();

  virtual void InitializeURLLoaderThrottleDelegate();

  virtual void SetPersistentCookieAccessSettings(PrefService* pref_service);

  // Returns the task runner that must be used for media IO.
  scoped_refptr<base::SingleThreadTaskRunner> GetMediaTaskRunner();

  // Gets object for enforcing video resolution policy restrictions.
  virtual media::VideoResolutionPolicy* GetVideoResolutionPolicy();

  // Creates a CmaBackendFactory.
  virtual media::CmaBackendFactory* GetCmaBackendFactory();

  media::MediaResourceTracker* media_resource_tracker();

  void ResetMediaResourceTracker();

  media::MediaPipelineBackendManager* media_pipeline_backend_manager();

  std::unique_ptr<::media::AudioManager> CreateAudioManager(
      ::media::AudioLogFactory* audio_log_factory) override;
  bool OverridesAudioManager() override;
  media::MediaCapsImpl* media_caps();

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
  // Create a BluetoothAdapter for WebBluetooth support.
  // TODO(slan): This further couples the browser to the Cast service. Remove
  // this once the dedicated Bluetooth service has been implemented.
  // (b/76155468)
  virtual scoped_refptr<device::BluetoothAdapterCast> CreateBluetoothAdapter();
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

  // chromecast::metrics::CastMetricsServiceDelegate implementation:
  void SetMetricsClientId(const std::string& client_id) override;
  void RegisterMetricsProviders(
      ::metrics::MetricsService* metrics_service) override;

  // Returns whether or not the remote debugging service should be started
  // on browser startup.
  virtual bool EnableRemoteDebuggingImmediately();

  // Note: These were originally part of ContentBrowserClient, but have been
  // lifted into this class as they're now only used by Chromecast. This is a
  // transitional step to avoid breakage in the internal downstream repository.
  virtual void RunServiceInstance(
      const service_manager::Identity& identity,
      mojo::PendingReceiver<service_manager::mojom::Service>* receiver);
  virtual std::optional<service_manager::Manifest> GetServiceManifestOverlay(
      std::string_view service_name);
  std::vector<service_manager::Manifest> GetExtraServiceManifests();
  std::vector<std::string> GetStartupServices();

  // content::ContentBrowserClient implementation:
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  bool IsHandledURL(const GURL& url) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  network::mojom::NetworkContext* GetSystemNetworkContext() override;
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  std::string GetApplicationLocale() override;
  void AllowCertificateError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool is_primary_main_frame_request,
      bool strict_enforcement,
      base::OnceCallback<void(content::CertificateRequestResultType)> callback)
      override;
  base::OnceClosure SelectClientCertificate(
      content::BrowserContext* browser_context,
      int process_id,
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;
  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const url::Origin& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access) override;
  // New Mojo bindings should be added to
  // cast_content_browser_client_receiver_bindings.cc, so that they go through
  // security review.
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;
  void BindMediaServiceReceiver(content::RenderFrameHost* render_frame_host,
                                mojo::GenericPendingReceiver receiver) override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
  mojo::Remote<::media::mojom::MediaService> RunSecondaryMediaService()
      override;
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
  void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
  std::unique_ptr<content::NavigationUIData> GetNavigationUIData(
      content::NavigationHandle* navigation_handle) override;
  bool ShouldEnableStrictSiteIsolation() override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(content::NavigationHandle* handle) override;
  void RegisterNonNetworkSubresourceURLLoaderFactories(
      int render_process_id,
      int render_frame_id,
      const std::optional<url::Origin>& request_initiator_origin,
      NonNetworkURLLoaderFactoryMap* factories) override;
  void OnNetworkServiceCreated(
      network::mojom::NetworkService* network_service) override;
  void ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override;
  std::string GetUserAgent() override;
  bool DoesSiteRequireDedicatedProcess(content::BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;
  bool IsWebUIAllowedToMakeNetworkRequests(const url::Origin& origin) override;
  PrivateNetworkRequestPolicyOverride ShouldOverridePrivateNetworkRequestPolicy(
      content::BrowserContext* browser_context,
      const url::Origin& origin) override;

  CastFeatureListCreator* GetCastFeatureListCreator() {
    return cast_feature_list_creator_;
  }

  void CreateGeneralAudienceBrowsingService();

  virtual std::unique_ptr<::media::CdmFactory> CreateCdmFactory(
      ::media::mojom::FrameInterfaceFactory* frame_interfaces);

#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void BindGpuHostReceiver(mojo::GenericPendingReceiver receiver) override;
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

  CastNetworkContexts* cast_network_contexts() {
    return cast_network_contexts_.get();
  }

 protected:
  explicit CastContentBrowserClient(
      CastFeatureListCreator* cast_feature_list_creator);

  CastBrowserMainParts* browser_main_parts() {
    return cast_browser_main_parts_;
  }

  void BindMediaRenderer(
      mojo::PendingReceiver<::media::mojom::Renderer> receiver);

  void GetApplicationMediaInfo(std::string* application_session_id,
                               bool* mixer_audio_enabled,
                               content::RenderFrameHost* render_frame_host);

  // Returns whether buffering should be used for the CMA Pipeline created for
  // this runtime instance. May be called from any thread.
  virtual bool IsBufferingEnabled();

 private:
  // Create device cert/key
  virtual scoped_refptr<net::X509Certificate> DeviceCert();
  virtual scoped_refptr<net::SSLPrivateKey> DeviceKey();

  virtual bool IsWhitelisted(const GURL& gurl,
                             const std::string& session_id,
                             int render_process_id,
                             int render_frame_id,
                             bool for_device_auth);

  void SelectClientCertificateOnIOThread(
      GURL requesting_url,
      const std::string& session_id,
      int render_process_id,
      int render_frame_id,
      scoped_refptr<base::SequencedTaskRunner> original_runner,
      base::OnceCallback<void(scoped_refptr<net::X509Certificate>,
                              scoped_refptr<net::SSLPrivateKey>)>
          continue_callback);

#if !BUILDFLAG(IS_FUCHSIA)
  // Returns the crash signal FD corresponding to the current process type.
  int GetCrashSignalFD(const base::CommandLine& command_line);

#if !BUILDFLAG(IS_ANDROID)
  // Creates a CrashHandlerHost instance for the given process type.
  breakpad::CrashHandlerHostLinux* CreateCrashHandlerHost(
      const std::string& process_type);

  // A static cache to hold crash_handlers for each process_type
  std::map<std::string, breakpad::CrashHandlerHostLinux*> crash_handlers_;

  // Notify renderers of memory pressure (Android renderers register directly
  // with OS for this).
  std::unique_ptr<MemoryPressureControllerImpl> memory_pressure_controller_;
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_FUCHSIA)

  // CMA thread used by AudioManager, MojoRenderer, and MediaPipelineBackend.
  std::unique_ptr<base::Thread> media_thread_;

  // Tracks usage of media resource by e.g. CMA pipeline, CDM.
  media::MediaResourceTracker* media_resource_tracker_ = nullptr;

#if BUILDFLAG(ENABLE_CAST_RENDERER)
  void CreateMediaService(
      mojo::PendingReceiver<::media::mojom::MediaService> receiver);

  // VideoGeometrySetterService must be constructed On a sequence, and later
  // runs and destructs on this sequence.
  void CreateVideoGeometrySetterServiceOnMediaThread();
  void BindVideoGeometrySetterServiceOnMediaThread(
      mojo::GenericPendingReceiver receiver);
  // video_geometry_setter_service_ lives on media thread.
  std::unique_ptr<media::VideoGeometrySetterService, base::OnTaskRunnerDeleter>
      video_geometry_setter_service_;
#endif

  // Created by CastContentBrowserClient but owned by BrowserMainLoop.
  CastBrowserMainParts* cast_browser_main_parts_;
  std::unique_ptr<CastNetworkContexts> cast_network_contexts_;
  std::unique_ptr<media::CmaBackendFactory> cma_backend_factory_;
  std::unique_ptr<GeneralAudienceBrowsingService>
      general_audience_browsing_service_;

  CastFeatureListCreator* cast_feature_list_creator_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_BROWSER_CLIENT_H_
