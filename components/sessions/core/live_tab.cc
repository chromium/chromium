// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/live_tab.h"

namespace sessions {

LiveTab::~LiveTab() {}

std::unique_ptr<PlatformSpecificTabData> LiveTab::GetPlatformSpecificTabData() {
  return nullptr;
}

}  // namespace sessions
