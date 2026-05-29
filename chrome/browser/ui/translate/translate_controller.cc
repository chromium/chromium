// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/translate_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

DEFINE_USER_DATA(TranslateController);

// static
TranslateController* TranslateController::From(BrowserWindowInterface* window) {
  return Get(window->GetUnownedUserDataHost());
}
