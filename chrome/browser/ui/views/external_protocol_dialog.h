// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_

#include <memory>

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

// Dialog that asks the user to confirm launching a url using the external
// protocol handler.
//
// Users can allow or block the launch and optionally remember this decision
// for the specific protocol and initiating origin.
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

  // Trigger input protection to protect against certain kinds of clickjacking.
  void TriggerInputProtection();

  // views::DialogDelegate:
  bool ShouldIgnoreButtonPressedEventHandling(
      View* button,
      const ui::Event& event) const override;
  bool ShouldAllowKeyEventsDuringInputProtection() const override;

  // Simulates Picture-in-Picture occlussion changed for testing.
  void SimulateOcclusionStateChangedForTesting(bool occluded);

  const base::WeakPtr<content::WebContents> web_contents_;

  const GURL url_;
  const std::u16string program_name_;
  const std::optional<url::Origin> initiating_origin_;
  const content::WeakDocumentPtr initiator_document_;

  // The message box whose commands we handle.
  raw_ptr<views::MessageBoxView> message_box_view_ = nullptr;

  // The PictureInPictureWatcher tracks dialog occlussions by Picture-in-Picture
  // windows, to ensure input protection and ignore spurious interactions.
  class PictureInPictureWatcher;
  std::unique_ptr<PictureInPictureWatcher> picture_in_picture_watcher_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTERNAL_PROTOCOL_DIALOG_H_
