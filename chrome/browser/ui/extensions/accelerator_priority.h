// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_ACCELERATOR_PRIORITY_H_
#define CHROME_BROWSER_UI_EXTENSIONS_ACCELERATOR_PRIORITY_H_

#include "ui/base/accelerators/accelerator_manager.h"

// The accelerator priority functions are intended to distinguish between
// accelerators that should preserve the built-in Chrome keybinding semantics
// (normal) and accelerators that should always override web page key handling
// (high). High priority is used for all accelerators assigned to extensions,
// which are extensions of the user agent and should (by default) supersede the
// browser shortcuts.
inline constexpr ui::AcceleratorManager::HandlerPriority
    kExtensionAcceleratorPriority = ui::AcceleratorManager::kHighPriority;

#endif  // CHROME_BROWSER_UI_EXTENSIONS_ACCELERATOR_PRIORITY_H_
