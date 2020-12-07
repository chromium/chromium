// Copyright 2020 The Chromium Authors. All Rights Reserved.
// Use of this source code is governed by the Apache v2.0 license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_PICKER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_PICKER_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace chromeos {

// The WebUI for chrome://emoji-picker
class EmojiPicker : public content::WebUIController {
 public:
  explicit EmojiPicker(content::WebUI* web_ui);
  ~EmojiPicker() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(EmojiPicker);
};

class EmojiPickerDialog : public ui::WebDialogDelegate {
 public:
  static void Show();
  ~EmojiPickerDialog() override;

 private:
  EmojiPickerDialog();
  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;

  content::WebUI* webui_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(EmojiPickerDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_PICKER_H_
