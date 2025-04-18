// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_layout_state.h"

#include "chrome/browser/ui/tabs/tab_types.h"

bool TabLayoutState::IsClosed() const {
  return openness_ == TabOpen::kClosed;
}
