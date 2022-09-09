// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_otr_state.h"

#include "chrome/browser/ui/browser_list.h"

namespace chrome {

bool IsOffTheRecordSessionActive() {
  return BrowserList::IsOffTheRecordBrowserActive();
}

}  // namespace chrome
