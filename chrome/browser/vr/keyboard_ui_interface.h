// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_KEYBOARD_UI_INTERFACE_H_
#define CHROME_BROWSER_VR_KEYBOARD_UI_INTERFACE_H_

#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/vr_base_export.h"

namespace vr {

// The keyboard communicates state changes to the VR UI via this interface. Note
// that we have this interface to restrict the UI API to keyboard-specific
// callback functions because the keyboard delegate doesn't need access to all
// of the UI.
class VR_BASE_EXPORT KeyboardUiInterface {
 public:
  virtual ~KeyboardUiInterface() {}
  virtual void OnInputEdited(const EditedText& info) = 0;
  virtual void OnInputCommitted(const EditedText& info) = 0;
  virtual void OnKeyboardHidden() = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_KEYBOARD_UI_INTERFACE_H_
