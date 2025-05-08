// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_id.h"

namespace tabs_api {

bool operator==(const TabId& a, const TabId& b) {
  return a.Type() == b.Type() && a.Id() == b.Id();
}

}  // namespace tabs_api
