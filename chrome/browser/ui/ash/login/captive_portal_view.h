// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_CAPTIVE_PORTAL_VIEW_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_CAPTIVE_PORTAL_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/simple_web_view.h"
#include "ui/views/controls/webview/simple_web_view_dialog_delegate.h"
#include "ui/views/view.h"

namespace ash {
class CaptivePortalWindowProxy;

class CaptivePortalView : public views::View,
                          public views::SimpleWebViewDialogDelegate {
  METADATA_HEADER(CaptivePortalView, views::View)

 public:
  CaptivePortalView(CaptivePortalWindowProxy* proxy,
                    const std::string& network_name);

  CaptivePortalView(const CaptivePortalView&) = delete;
  CaptivePortalView& operator=(const CaptivePortalView&) = delete;

  ~CaptivePortalView() override;

  // Starts loading.
  void StartLoad();

  // Initializes the view. Should be attached to a Widget before call.
  void Init();

  views::SimpleWebView* simple_web_view() { return simple_web_view_; }
  views::View* dialog_view() { return simple_web_view_->GetView(); }

  // SimpleWebViewDialogDelegate:
  void OnNavigationStateChanged(
      content::WebContents* source,
      content::InvalidateTypes changed_flags) override;
  void OnLoadingStateChanged(content::WebContents* source,
                             bool to_different_document) override;
  std::unique_ptr<views::WidgetDelegate> MakeWidgetDelegate(
      std::unique_ptr<views::WidgetDelegate> base_delegate) override;

 private:
  // Contains CaptivePortalWindowProxy to be notified when redirection state is
  // resolved.
  raw_ptr<CaptivePortalWindowProxy, DanglingUntriaged> proxy_;

  raw_ptr<views::SimpleWebView> simple_web_view_ = nullptr;

  GURL start_url_;
  const std::string network_name_;
  bool redirected_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_CAPTIVE_PORTAL_VIEW_H_
