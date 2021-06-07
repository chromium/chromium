// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_GET_CURRENT_BROWSING_CONTEXT_MEDIA_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_GET_CURRENT_BROWSING_CONTEXT_MEDIA_DIALOG_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/window/dialog_delegate.h"

class GetCurrentBrowsingContextMediaDialog : public DesktopMediaPicker {
 public:
  GetCurrentBrowsingContextMediaDialog();
  ~GetCurrentBrowsingContextMediaDialog() override;

  void NotifyDialogResult(const content::DesktopMediaID& source);

  // DesktopMediaPicker:
  void Show(const DesktopMediaPicker::Params& params,
            std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
            DoneCallback done_callback) override;

  views::BubbleDialogModelHost* GetHostForTesting() {
    return dialog_model_host_for_testing_;
  }

 private:
  // Used to create the dialog using ui::DialogModel.
  std::unique_ptr<views::DialogDelegate> CreateDialogHost(
      const DesktopMediaPicker::Params& params);

  // This method is used as a callback to support automatically accepting or
  // rejecting tab-capture through getCurrentBrowserContextMedia in tests.
  // It acts as though the user had accepted/rejected the capture-request.
  void MaybeAutomateUserInput();

  // These flags are used for testing and make an automatic selection
  // without the user's input.
  const bool auto_accept_tab_capture_for_testing_;
  const bool auto_reject_tab_capture_for_testing_;

  DoneCallback callback_;
  views::BubbleDialogModelHost* dialog_model_host_for_testing_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GetCurrentBrowsingContextMediaDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_GET_CURRENT_BROWSING_CONTEXT_MEDIA_DIALOG_H_
