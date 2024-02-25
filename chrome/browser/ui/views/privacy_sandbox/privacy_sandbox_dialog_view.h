// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_VIEW_H_

#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class Browser;

namespace content {
class WebContents;
}

namespace views {
class WebView;
}

// Implements the PrivacySandboxDialog as a View. The view contains a WebView
// into which is loaded a WebUI page which renders the actual dialog content.
class PrivacySandboxDialogView : public views::View {
  METADATA_HEADER(PrivacySandboxDialogView, views::View)

 public:
  PrivacySandboxDialogView(Browser* browser,
                           PrivacySandboxService::PromptType dialog_type);

  void Close();

 private:
  void ResizeNativeView(int height);
  void ShowNativeView();
  void OpenPrivacySandboxSettings();
  void OpenPrivacySandboxAdMeasurementSettings();
  friend class PrivacySandboxDialogViewBrowserTest;
  content::WebContents* GetWebContentsForTesting();

  raw_ptr<views::WebView> web_view_;
  raw_ptr<Browser> browser_;
  base::TimeTicks dialog_created_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_VIEW_H_
