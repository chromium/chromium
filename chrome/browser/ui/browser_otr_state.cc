// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_otr_state.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"

bool IsOffTheRecordSessionActive() {
  bool has_active_off_the_record_browser = false;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        has_active_off_the_record_browser =
            browser->GetProfile()->IsOffTheRecord();
        return !has_active_off_the_record_browser;
      });
  return has_active_off_the_record_browser;
}
