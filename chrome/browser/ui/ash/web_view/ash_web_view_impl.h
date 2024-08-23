// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WEB_VIEW_ASH_WEB_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_ASH_WEB_VIEW_ASH_WEB_VIEW_IMPL_H_

#include "ash/public/cpp/ash_web_view.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace views {
class WebView;
}  // namespace views

namespace gfx {
class RoundedCornersF;
}  // namespace gfx

// Implements AshWebView used by Ash to work around dependency
// restrictions.
class AshWebViewImpl : public ash::AshWebView,
                       public content::WebContentsDelegate,
                       public content::WebContentsObserver {
  METADATA_HEADER(AshWebViewImpl, ash::AshWebView)
 public:
  explicit AshWebViewImpl(const InitParams& params);
  ~AshWebViewImpl() override;

  AshWebViewImpl(AshWebViewImpl&) = delete;
  AshWebViewImpl& operator=(AshWebViewImpl&) = delete;

  // ash::AshWebView:
  gfx::NativeView GetNativeView() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void Layout(PassKey) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool GoBack() override;
  void Navigate(const GURL& url) override;
  const GURL& GetVisibleURL() override;
  bool IsErrorDocument() override;
  void AddedToWidget() override;
  views::View* GetInitiallyFocusedView() override;
  void SetCornerRadii(const gfx::RoundedCornersF& corner_radii) override;
  const base::UnguessableToken& GetMediaSessionRequestId() override;

  // content::WebContentsDelegate:
  bool IsWebContentsCreationOverridden(
      content::SiteInstance* source_site_instance,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override;
  bool TakeFocus(content::WebContents* web_contents, bool reverse) override;
  void NavigationStateChanged(content::WebContents* web_contents,
                              content::InvalidateTypes changed_flags) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  std::string GetTitleForMediaControls(
      content::WebContents* web_contents) override;

  // content::WebContentsObserver:
  void DidStopLoading() override;
  void OnFocusChangedInPage(content::FocusedNodeDetails* details) override;
  void PrimaryPageChanged(content::Page& page) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void NavigationEntriesDeleted() override;

 private:
  void InitWebContents(Profile* profile);
  void InitLayout(Profile* profile);

  void NotifyDidSuppressNavigation(const GURL& url,
                                   WindowOpenDisposition disposition,
                                   bool from_user_gesture);

  void UpdateCanGoBack();

  // Update the window property that stores whether we can minimize on a back
  // event.
  void UpdateMinimizeOnBackProperty();

  const InitParams params_;

  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<views::WebView> web_view_ = nullptr;

  // Whether or not the embedded |web_contents_| can go back.
  bool can_go_back_ = false;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<AshWebViewImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_WEB_VIEW_ASH_WEB_VIEW_IMPL_H_
