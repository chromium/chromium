// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_DIALOG_H_

#include "base/macros.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

#include "chrome/browser/ui/webui/chromeos/emoji/emoji_handler.h"

namespace chromeos {

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

  // Window for the emoji picker.  Used by the handler to close the window.
  static gfx::NativeWindow window;
  friend class EmojiHandler;

  DISALLOW_COPY_AND_ASSIGN(EmojiPickerDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_DIALOG_H_
