// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_SIGNIN_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_SIGNIN_PAGE_VIEW_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "chrome/browser/ui/views/webid/webid_dialog_views.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class GURL;

namespace views {
class WebView;
}  // namespace views

// The WebIdSigninWindow loads IDP sign-in page in a modal allowing user to
// sign in. The modal may be closed by user or once IDP sign-in page has
// completed its process and have called the appropriate JS callback.
class SigninPageView : public views::View, public content::WebContentsObserver {
 public:
  METADATA_HEADER(SigninPageView);
  SigninPageView(WebIdDialogViews* dialog,
                 content::WebContents* initiator_web_contents,
                 content::WebContents* idp_web_contents,
                 const GURL& provider);
  ~SigninPageView() override = default;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void LoadProgressChanged(double progress) override;
  void TitleWasSet(content::NavigationEntry* entry) override;

 private:
  std::unique_ptr<views::WebView> CreateContentWebView(
      content::WebContents* idp_web_contents,
      const GURL& provider);
  std::unique_ptr<views::View> CreateHeaderView();

  void UpdateHeaderView();

  // The dialog that is hosting this view.
  WebIdDialogViews* dialog_;

  content::WebContents* initiator_web_contents_;
  // The header of the dialog, owned by the view hierarchy.
  views::View* header_view_;
  // The contents of the dialog, owned by the view hierarchy.
  views::WebView* web_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_WEBID_SIGNIN_PAGE_VIEW_H_
