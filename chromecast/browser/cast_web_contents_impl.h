// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_CONTENTS_IMPL_H_
#define CHROMECAST_BROWSER_CAST_WEB_CONTENTS_IMPL_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromecast/bindings/public/mojom/api_bindings.mojom.h"
#include "chromecast/browser/cast_media_blocker.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/mojom/cast_web_service.mojom.h"
#include "chromecast/browser/named_message_port_connector_cast.h"
#include "chromecast/mojo/remote_interfaces.h"
#include "components/media_control/browser/media_blocker.h"
#include "components/on_load_script_injector/browser/on_load_script_injector_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/media_playback_renderer_type.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace chromecast {

namespace shell {
class RemoteDebuggingServer;
}  // namespace shell

class CastWebContentsImpl : public CastWebContents,
                            public content::RenderProcessHostObserver,
                            public content::WebContentsObserver {
 public:
  CastWebContentsImpl(content::WebContents* web_contents,
                      mojom::CastWebViewParamsPtr params);

  CastWebContentsImpl(const CastWebContentsImpl&) = delete;
  CastWebContentsImpl& operator=(const CastWebContentsImpl&) = delete;

  ~CastWebContentsImpl() override;

  content::WebContents* web_contents() const override;
  PageState page_state() const override;
  const media_control::MediaBlocker* media_blocker() const override;

  // CastWebContents implementation:
  int tab_id() const override;
  int id() const override;
  void SetAppProperties(const std::string& app_id,
                        const std::string& session_id,
                        bool is_audio_app,
                        const GURL& app_web_url,
                        bool enforce_feature_permissions,
                        const std::vector<int32_t>& feature_permissions,
                        const std::vector<std::string>&
                            additional_feature_permission_origins) override;
  void SetGroupInfo(const std::string& session_id,
                    bool is_multizone_launch) override;
  void AddRendererFeatures(base::Value::Dict features) override;
  void SetInterfacesForRenderer(
      mojo::PendingRemote<mojom::RemoteInterfaces> remote_interfaces) override;
  void LoadUrl(const GURL& url) override;
  void ClosePage() override;
  void Stop(int error_code) override;
  void SetWebVisibilityAndPaint(bool visible) override;
  bool TryBindReceiver(mojo::GenericPendingReceiver& receiver) override;
  InterfaceBundle* local_interfaces() override;
  void BlockMediaLoading(bool blocked) override;
  void BlockMediaStarting(bool blocked) override;
  void EnableBackgroundVideoPlayback(bool enabled) override;
  void AddBeforeLoadJavaScript(uint64_t id, std::string_view script) override;
  void PostMessageToMainFrame(
      const std::string& target_origin,
      const std::string& data,
      std::vector<blink::WebMessagePort> ports) override;
  void ExecuteJavaScript(
      const std::u16string& javascript,
      base::OnceCallback<void(base::Value)> callback) override;
  void ConnectToBindingsService(
      mojo::PendingRemote<mojom::ApiBindings> api_bindings_remote) override;
  void SetEnabledForRemoteDebugging(bool enabled) override;
  void GetMainFramePid(GetMainFramePidCallback cb) override;
  bool is_websql_enabled() override;
  bool is_mixer_audio_enabled() override;

  // content::RenderProcessHostObserver implementation:
  void RenderProcessReady(content::RenderProcessHost* host) override;
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // content::WebContentsObserver implementation:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;
  void InnerWebContentsCreated(
      content::WebContents* inner_web_contents) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void WebContentsDestroyed() override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;
  void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                           const content::MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id,
      content::WebContentsObserver::MediaStoppedReason reason) override;

 private:
  // Constructor used to create inner CastWebContents.
  CastWebContentsImpl(content::WebContents* web_contents,
                      mojom::CastWebViewParamsPtr params,
                      CastWebContents* parent);
  void OnPageLoading();
  void OnPageLoaded();
  void UpdatePageState();
  void NotifyPageState();
  void TracePageLoadBegin(const GURL& url);
  void TracePageLoadEnd(const GURL& url);
  void DisableDebugging();
  void OnClosePageTimeout();
  void RemoveRenderProcessHostObserver();
  std::vector<chromecast::shell::mojom::FeaturePtr> GetRendererFeatures();
  void OnBindingsReceived(
      std::vector<chromecast::mojom::ApiBindingPtr> bindings);
  bool OnPortConnected(std::string_view port_name,
                       std::unique_ptr<cast_api_bindings::MessagePort> port);

  content::WebContents* web_contents_;
  mojom::CastWebViewParamsPtr params_;
  PageState page_state_;
  PageState last_state_;
  shell::RemoteDebuggingServer* const remote_debugging_server_;
  std::unique_ptr<CastMediaBlocker> media_blocker_;
  std::optional<std::vector<std::string>> activity_url_filter_;

  // Retained so that this observer can be removed before being destroyed:
  content::RenderProcessHost* main_process_host_;

  CastWebContents* const parent_cast_web_contents_ = nullptr;
  base::flat_set<std::unique_ptr<CastWebContents>> inner_contents_;
  base::Value::Dict renderer_features_;

  const int tab_id_;
  const int id_;
  base::TimeTicks start_loading_ticks_;

  // True once the main frame finishes loading and there are no outstanding
  // navigations.
  bool main_frame_loaded_;
  content::NavigationHandle* active_navigation_ = nullptr;

  bool closing_;
  bool stopped_;
  bool stop_notified_;
  bool notifying_;
  int last_error_;

  on_load_script_injector::OnLoadScriptInjectorHost<uint64_t> script_injector_;
  mojo::Remote<mojom::ApiBindings> api_bindings_;

  // If |ConnectToBindingsService| is invoked, |bindings_received_| is set
  // false. Following |LoadUrl| will be stored in |pending_load_url_|, and
  // will be invoked once bindings are received.
  bool bindings_received_{false};
  GURL pending_load_url_;

  // Used to open a MessageChannel for connecting API bindings.
  std::unique_ptr<NamedMessagePortConnectorCast> named_message_port_connector_;

  base::ObserverList<CastWebContentsObserver>::Unchecked observer_list_;

  InterfaceBundle local_interfaces_;
  RemoteInterfaces remote_interfaces_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastWebContentsImpl> weak_factory_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_CONTENTS_IMPL_H_
