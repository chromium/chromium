// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_POLICY_ENTERPRISE_STARTUP_DIALOG_MAC_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_POLICY_ENTERPRISE_STARTUP_DIALOG_MAC_UTIL_H_

#import "ui/gfx/native_widget_types.h"

namespace policy {

// Run the modal dialog loop. Function is blocked until |StopModal| is called.
void StartModal(gfx::NativeWindow dialog);

// Stop the modal dialog loop. |StartModal| will return after this function is
// called.
void StopModal();

}  // namespace policy

#endif  // CHROME_BROWSER_UI_VIEWS_POLICY_ENTERPRISE_STARTUP_DIALOG_MAC_UTIL_H_
