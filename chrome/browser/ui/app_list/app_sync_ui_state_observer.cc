// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_sync_ui_state_observer.h"

#include "base/check.h"

AppSyncUIStateObserver::~AppSyncUIStateObserver() {
  CHECK(!IsInObserverList());
}
