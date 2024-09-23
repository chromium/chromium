// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/weak_document_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
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

class ExternalProtocolDialog : public views::DialogDelegateView {
  METADATA_HEADER(ExternalProtocolDialog, views::DialogDelegateView)

 public:
  // Show by calling ExternalProtocolHandler::RunExternalProtocolDialog().
  ExternalProtocolDialog(content::WebContents* web_contents,
                         const GURL& url,
                         const std::u16string& program_name,
                         const std::optional<url::Origin>& initiating_origin,
                         content::WeakDocumentPtr initiator_document);
  ExternalProtocolDialog(const ExternalProtocolDialog&) = delete;
  ExternalProtocolDialog& operator=(const ExternalProtocolDialog&) = delete;
  ~ExternalProtocolDialog() override;

  // views::DialogDelegateView:
  bool ShouldShowCloseButton() const override;
  std::u16string GetWindowTitle() const override;

 private:
  friend class test::ExternalProtocolDialogTestApi;

  void SetRememberSelectionCheckboxCheckedForTesting(bool checked);
  void OnDialogAccepted();

  const base::WeakPtr<content::WebContents> web_contents_;

  const GURL url_;
  const std::u16string program_name_;
  const std::optional<url::Origin> initiating_origin_;
  const content::WeakDocumentPtr initiator_document_;

  // The message box whose commands we handle.
  raw_ptr<views::MessageBoxView> message_box_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
