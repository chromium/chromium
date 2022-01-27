// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_VIEW_H_

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"

class Browser;

// Implements the PrivacySandboxDialog as a View. The view contains a WebView
// into which is loaded a WebUI page which renders the actual dialog content.
class PrivacySandboxDialogView : public views::DialogDelegateView {
 public:
  METADATA_HEADER(PrivacySandboxDialogView);
  PrivacySandboxDialogView(Browser* browser,
                           PrivacySandboxService::DialogType dialog_type);

  void Close();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_VIEW_H_
