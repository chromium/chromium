// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_WEB_CONTENTS_IMPL_H_
#define CHROMECAST_BROWSER_CAST_WEB_CONTENTS_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_media_blocker.h"
#include "chromecast/browser/cast_web_contents.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace chromecast {

namespace shell {
class RemoteDebuggingServer;
}  // namespace shell

class CastWebContentsImpl : public CastWebContents,
                            public content::WebContentsObserver {
 public:
  CastWebContentsImpl(content::WebContents* web_contents,
                      const InitParams& init_params);
  ~CastWebContentsImpl() override;

  content::WebContents* web_contents() const override;
  PageState page_state() const override;

  // CastWebContents implementation:
  int tab_id() const override;
  void AddRendererFeatures(std::vector<RendererFeature> features) override;
  void AllowWebAndMojoWebUiBindings() override;
  void ClearRenderWidgetHostView() override;
  void LoadUrl(const GURL& url) override;
  void ClosePage() override;
  void Stop(int error_code) override;
  void RegisterInterfaceProvider(
      const InterfaceSet& interface_set,
      service_manager::InterfaceProvider* interface_provider) override;
  service_manager::BinderRegistry* binder_registry() override;
  void BlockMediaLoading(bool blocked) override;
  void BlockMediaStarting(bool blocked) override;
  void EnableBackgroundVideoPlayback(bool enabled) override;
  void AddBeforeLoadJavaScript(base::StringPiece id,
                               const std::vector<std::string>& origins,
                               base::StringPiece script) override;
  void RemoveBeforeLoadJavaScript(base::StringPiece id) override;
  void PostMessageToMainFrame(
      const std::string& target_origin,
      const std::string& data,
      std::vector<mojo::ScopedMessagePipeHandle> channels) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // content::WebContentsObserver implementation:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void OnInterfaceRequestFromFrame(
      content::RenderFrameHost* /* render_frame_host */,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override;
  void MainFrameWasResized(bool width_changed) override;
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const content::mojom::ResourceLoadInfo& resource_load_info) override;
  void InnerWebContentsCreated(
      content::WebContents* inner_web_contents) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidFirstVisuallyNonEmptyPaint() override;
  void WebContentsDestroyed() override;
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;
  void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                           const content::MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id,
      content::WebContentsObserver::MediaStoppedReason reason) override;

 private:
  struct OriginScopedScript {
    OriginScopedScript();
    OriginScopedScript(const std::vector<std::string>& origins,
                       std::string script);
    OriginScopedScript& operator=(OriginScopedScript&& other);
    ~OriginScopedScript();

    const std::vector<std::string>& origins() const { return origins_; }
    const std::string script() const { return script_; }

    std::vector<std::string> origins_;
    std::string script_;

    DISALLOW_COPY_AND_ASSIGN(OriginScopedScript);
  };

  void OnPageLoading();
  void OnPageLoaded();
  void UpdatePageState();
  void NotifyPageState();
  void TracePageLoadBegin(const GURL& url);
  void TracePageLoadEnd(const GURL& url);
  void DisableDebugging();
  void OnClosePageTimeout();
  std::vector<chromecast::shell::mojom::FeaturePtr> GetRendererFeatures();

  content::WebContents* web_contents_;
  base::WeakPtr<Delegate> delegate_;
  PageState page_state_;
  PageState last_state_;
  const bool enabled_for_dev_;
  bool use_cma_renderer_;
  const bool handle_inner_contents_;
  BackgroundColor view_background_color_;
  shell::RemoteDebuggingServer* const remote_debugging_server_;
  std::unique_ptr<CastMediaBlocker> media_blocker_;

  base::flat_set<std::unique_ptr<CastWebContents>> inner_contents_;
  std::vector<RendererFeature> renderer_features_;

  const int tab_id_;
  base::TimeTicks start_loading_ticks_;
  bool main_frame_loaded_;
  bool closing_;
  bool stopped_;
  bool stop_notified_;
  bool notifying_;
  int last_error_;

  std::map<std::string, OriginScopedScript> before_load_scripts_;
  std::vector<std::string> before_load_scripts_order_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  service_manager::BinderRegistry binder_registry_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Map of InterfaceSet -> InterfaceProvider pointer.
  base::flat_map<InterfaceSet, service_manager::InterfaceProvider*>
      interface_providers_map_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CastWebContentsImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastWebContentsImpl);
};

std::ostream& operator<<(std::ostream& os,
                         CastWebContentsImpl::PageState state);

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_WEB_CONTENTS_IMPL_H_
