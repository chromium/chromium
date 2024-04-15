// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAD_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SAD_TAB_HELPER_H_

#include <memory>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class SadTab;

// Per-tab class to manage sad tab views. The sad tab view appears when the main
// frame of a WebContents has crashed. The behaviour depends on whether
// content::ShouldSkipEarlyCommitPendingForCrashedFrame is true or not.
//
// TODO(crbug.com/40052076): The early commit path is being removed, tidy
// these docs when that happens.
//
// If we are doing the early commit then the sad tab is removed when
// WebContentsObserver::RenderViewReady is signalled and does not come back
// unless the new frame also crashes.
//
// If we are not doing the early commit then the sad tab is removed when the new
// frame is created but the new frame is left invisible, this leaves the empty
// WebContents displaying. If the new frame commits, it becomes visible. If the
// commit is aborted, we reinstate the sad tab.
//
class SadTabHelper : public content::WebContentsObserver,
                     public content::WebContentsUserData<SadTabHelper> {
 public:
  SadTabHelper(const SadTabHelper&) = delete;
  SadTabHelper& operator=(const SadTabHelper&) = delete;

  ~SadTabHelper() override;

  SadTab* sad_tab() { return sad_tab_.get(); }

  // Called when the sad tab needs to be reinstalled in the WebView,
  // for example because a tab was activated, or because a tab was
  // dragged to a new browser window.
  void ReinstallInWebView();

 private:
  friend class content::WebContentsUserData<SadTabHelper>;

  explicit SadTabHelper(content::WebContents* web_contents);

  void InstallSadTab(base::TerminationStatus status);

  // Overridden from content::WebContentsObserver:
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void RenderViewReady() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  std::unique_ptr<SadTab> sad_tab_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SAD_TAB_HELPER_H_
