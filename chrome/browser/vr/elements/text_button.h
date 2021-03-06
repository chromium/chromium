// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXT_BUTTON_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXT_BUTTON_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/elements/text.h"

namespace vr {

// TextButton is a Button that sizes itself to a supplied text string.
class TextButton : public Button {
 public:
  TextButton(float text_height, AudioDelegate* audio_delegate);
  ~TextButton() override;

  void SetText(const base::string16& text);

 private:
  void OnSetColors(const ButtonColors& colors) override;

  Text* text_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TextButton);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_BUTTON_H_
