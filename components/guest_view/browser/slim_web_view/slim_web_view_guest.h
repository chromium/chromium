// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_GUEST_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_GUEST_H_

#include "components/guest_view/browser/guest_view.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_permission_helper.h"
#include "net/base/net_errors.h"

class GURL;

namespace guest_view {

// A minimal implementation of GuestView for embedding web content in Chrome.
// This implementation is used in Android and does not make use of the
// extensions infrastructure. As such, it can only be used in internal pages and
// it doesn't support the whole suite of APIs that <webview> does.
class SlimWebViewGuest : public GuestView<SlimWebViewGuest> {
 public:
  static constexpr char Type[] = "SlimWebView";
  static const guest_view::GuestViewHistogramValue HistogramValue;

  static std::unique_ptr<GuestViewBase> Create(
      content::RenderFrameHost* owner_render_frame_host);

  SlimWebViewGuest(const SlimWebViewGuest&) = delete;
  SlimWebViewGuest& operator=(const SlimWebViewGuest&) = delete;
  ~SlimWebViewGuest() override;

  SlimWebViewPermissionHelper& permission_helper() {
    return permission_helper_;
  }

  void Navigate(const GURL& url);

 private:
  explicit SlimWebViewGuest(content::RenderFrameHost* owner_render_frame_host);

  // content::GuestPageHolder::Delegate:
  bool GuestHandleContextMenu(content::RenderFrameHost& render_frame_host,
                              const content::ContextMenuParams& params) final;

  // content::WebContentsDelegate:
  bool IsWebContentsCreationOverridden(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) final;
  content::WebContents* CreateCustomWebContents(
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      bool is_new_browsing_instance,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      const content::StoragePartitionConfig& partition_config,
      content::SessionStorageNamespace* session_storage_namespace) final;
  void RendererUnresponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) final;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) final;

  // content::WebContentsObserver:
  void DidStartNavigation(content::NavigationHandle* navigation_handle) final;
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) final;

  // guest_view::GuestViewBase:
  const char* GetAPINamespace() const final;
  int GetTaskPrefix() const final;
  void GuestViewDocumentOnLoadCompleted() final;
  bool IsAutoSizeSupported() const final;
  void GuestSizeChangedDueToAutoSize(const gfx::Size& old_size,
                                     const gfx::Size& new_size) final;
  void GuestViewMainFrameProcessGone(base::TerminationStatus status) final;
  void GuestRequestMediaAccessPermission(
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) final;
  void MaybeRecreateGuestContents(
      content::RenderFrameHost* outer_contents_frame) final;
  void CreateInnerPage(std::unique_ptr<GuestViewBase> owned_this,
                       scoped_refptr<content::SiteInstance> site_instance,
                       const base::DictValue& create_params,
                       GuestPageCreatedCallback callback) final;
  void GuestViewDidStopLoading() final;

  void LoadAbort(bool is_top_level, const GURL& url, net::Error error_code);

  SlimWebViewPermissionHelper permission_helper_{this};
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_GUEST_H_
