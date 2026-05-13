// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary SetCurrentInputMethodOptions {
  // The input method ID to set as current input method. This input
  // method has to be enabled by enterprise policies. Supported IDs
  // are located in https://crsrc.org/c/chrome/browser/resources/chromeos/input_method.
  required DOMString inputMethodId;
};

// Use the <code>chrome.enterprise.kioskInput</code> API to change input
// settings for Kiosk sessions.
// Note: This API is only available to extensions installed by enterprise
// policy in ChromeOS Kiosk sessions.
[platforms = ("chromeos"),
 implemented_in = "chrome/browser/extensions/api/enterprise_kiosk_input/enterprise_kiosk_input_api.h"]
interface KioskInput {
  // Sets the current input method. This function only changes
  // the current input method to an enabled input method.
  // Input methods can be enabled by enterprise polices.
  // If the input method ID is invalid, or not enabled,
  // $(ref:runtime.lastError) will be set with a failure reason.
  // |options|: Object containing the fields defined in
  //            $(ref:SetCurrentInputMethodOptions).
  // |Returns|: Returns a Promise which resolves when the input method is
  // changed, or rejects if there is an error.
  static Promise<undefined> setCurrentInputMethod(
      SetCurrentInputMethodOptions options);
};

partial interface Enterprise {
  static attribute KioskInput kioskInput;
};

partial interface Browser {
  static attribute Enterprise enterprise;
};
