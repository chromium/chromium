// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
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

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_EMOJI_EMOJI_PICKER_H_
