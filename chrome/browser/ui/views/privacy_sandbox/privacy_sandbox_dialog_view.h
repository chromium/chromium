// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_VIEW_H_

#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_ui.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace views {
class WebView;
}

// Implements the PrivacySandboxDialog as a View. The view contains a WebView
// into which is loaded a WebUI page which renders the actual dialog content.
class PrivacySandboxDialogView : public views::View,
                                 public privacy_sandbox::BaseDialogUIDelegate {
  METADATA_HEADER(PrivacySandboxDialogView, views::View)

 public:
  static std::unique_ptr<PrivacySandboxDialogView>
  CreateDialogViewForPromptType(BrowserWindowInterface* browser,
                                PrivacySandboxService::PromptType prompt_type);
  // TODO(chrstne): Create initialization method for PSNotice, v2.

  content::WebContents* GetWebContentsForTesting();

  // privacy_sandbox::BaseDialogUIDelegate
  void CloseNativeView() override;
  void ResizeNativeView(int height) override;
  void ShowNativeView() override;

 private:
  friend class PrivacySandboxQueueTestNotice;

  explicit PrivacySandboxDialogView(BrowserWindowInterface* browser);
  void InitializeDialogUIForPromptType(
      PrivacySandboxService::PromptType prompt_type);
  void AdsDialogNoArgsCallback(
      PrivacySandboxService::AdsDialogCallbackNoArgsEvents event);
  void OpenPrivacySandboxSettings();
  void OpenPrivacySandboxAdMeasurementSettings();

  raw_ptr<views::WebView> web_view_;
  raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_VIEW_H_
