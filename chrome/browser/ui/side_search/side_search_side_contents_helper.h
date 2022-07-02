// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_SIDE_CONTENTS_HELPER_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_SIDE_CONTENTS_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class NavigationThrottle;
class WebContents;
}  // namespace content

class GURL;
class SideSearchConfig;

// Side Search helper for the WebContents hosted in the side panel.
class SideSearchSideContentsHelper
    : public content::WebContentsObserver,
      public content::WebContentsDelegate,
      public content::WebContentsUserData<SideSearchSideContentsHelper> {
 public:
  class Delegate {
   public:
    // Called by the side contents helper to navigate its associated tab
    // contents.
    virtual void NavigateInTabContents(
        const content::OpenURLParams& params) = 0;

    // Called when the last search URL encountered by the side panel has been
    // updated.
    virtual void LastSearchURLUpdated(const GURL& url) = 0;

    // Called when the side panel contents has terminated.
    virtual void SidePanelProcessGone() = 0;

    // Passthrough for the side content's WebContentsDelegate.
    virtual bool HandleKeyboardEvent(
        content::WebContents* source,
        const content::NativeWebKeyboardEvent& event);

    // Passthrough for the side content's WebContentsDelegate.
    virtual content::WebContents* OpenURLFromTab(
        content::WebContents* source,
        const content::OpenURLParams& params);
  };

  // Will call MaybeRecordMetricsPerJourney().
  ~SideSearchSideContentsHelper() override;

  // Maybe installs a throttle for the given navigation.
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* handle);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // content::WebContentsDelegate:
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::DragOperationsMask operations_allowed) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;

  // Navigates the associated tab contents to `url`.
  void NavigateInTabContents(const content::OpenURLParams& params);

  // Loads the `url` in the side contents, applying any additional headers as
  // necessary. Also calls MaybeRecordMetricsPerJourney().
  void LoadURL(const GURL& url);

  // Called to set the tab contents associated with this side panel contents.
  // The tab contents will always outlive this helper and its associated side
  // contents.
  void SetDelegate(Delegate* delegate);

 private:
  friend class content::WebContentsUserData<SideSearchSideContentsHelper>;
  explicit SideSearchSideContentsHelper(content::WebContents* web_contents);

  // Emits metrics data for the previous user journey if present.
  void MaybeRecordMetricsPerJourney();

  SideSearchConfig* GetConfig();

  // `delegate_` will outlive the SideContentsWrapper.
  raw_ptr<Delegate> delegate_ = nullptr;

  WebuiLoadTimer webui_load_timer_;

  // Only used for metrics.
  int navigation_within_side_search_count_ = 0;
  int redirection_to_tab_count_ = 0;
  bool has_loaded_url_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_SIDE_CONTENTS_HELPER_H_
