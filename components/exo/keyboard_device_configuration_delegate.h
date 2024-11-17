// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_KEYBOARD_DEVICE_CONFIGURATION_DELEGATE_H_
#define COMPONENTS_EXO_KEYBOARD_DEVICE_CONFIGURATION_DELEGATE_H_

namespace exo {

// Used as an extension to the KeyboardDelegate.
class KeyboardDeviceConfigurationDelegate {
 public:
  // Called when used keyboard type changed.
  virtual void OnKeyboardTypeChanged(bool is_physical) = 0;

 protected:
  virtual ~KeyboardDeviceConfigurationDelegate() = default;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_KEYBOARD_DEVICE_CONFIGURATION_DELEGATE_H_
