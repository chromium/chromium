// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class WebContents;
}

namespace test {
class ExternalProtocolDialogTestApi;
}

namespace views {
class MessageBoxView;
}

class ExternalProtocolDialog : public views::DialogDelegateView,
                               public content::WebContentsObserver {
 public:
  METADATA_HEADER(ExternalProtocolDialog);
  // Show by calling ExternalProtocolHandler::RunExternalProtocolDialog().
  ExternalProtocolDialog(content::WebContents* web_contents,
                         const GURL& url,
                         const std::u16string& program_name,
                         const base::Optional<url::Origin>& initiating_origin);
  ExternalProtocolDialog(const ExternalProtocolDialog&) = delete;
  ExternalProtocolDialog& operator=(const ExternalProtocolDialog&) = delete;
  ~ExternalProtocolDialog() override;

  // views::DialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  bool ShouldShowCloseButton() const override;
  std::u16string GetWindowTitle() const override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

 private:
  friend class test::ExternalProtocolDialogTestApi;

  void SetRememberSelectionCheckboxCheckedForTesting(bool checked);
  void OnDialogAccepted();

  const GURL url_;
  const std::u16string program_name_;
  const base::Optional<url::Origin> initiating_origin_;

  // The message box whose commands we handle.
  views::MessageBoxView* message_box_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
