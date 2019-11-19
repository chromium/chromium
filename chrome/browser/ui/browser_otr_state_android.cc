// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_otr_state.h"

namespace chrome {

bool IsIncognitoSessionActive() {
  return TabModelList::IsOffTheRecordSessionActive();
}

}  // namespace chrome
