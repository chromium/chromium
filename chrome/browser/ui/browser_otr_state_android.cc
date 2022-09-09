// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_otr_state.h"

namespace chrome {

bool IsOffTheRecordSessionActive() {
  return TabModelList::IsOffTheRecordSessionActive();
}

}  // namespace chrome
