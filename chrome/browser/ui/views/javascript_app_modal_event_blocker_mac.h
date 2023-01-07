// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_EVENT_BLOCKER_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_EVENT_BLOCKER_MAC_H_

#include "ui/gfx/native_widget_types.h"

// TODO(weili): Implement the proper app modal behaviors.
class JavascriptAppModalEventBlockerMac {
 public:
  explicit JavascriptAppModalEventBlockerMac(
      gfx::NativeWindow app_modal_window) {}
  JavascriptAppModalEventBlockerMac(const JavascriptAppModalEventBlockerMac&) =
      delete;
  JavascriptAppModalEventBlockerMac& operator=(
      const JavascriptAppModalEventBlockerMac&) = delete;
  ~JavascriptAppModalEventBlockerMac() = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_EVENT_BLOCKER_MAC_H_
