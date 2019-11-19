// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/browser/ns_view_ids.h"

namespace remote_cocoa {

uint64_t GetNewNSViewId() {
  static uint64_t counter = 0;
  counter += 1;
  return counter;
}

}  // namespace remote_cocoa
