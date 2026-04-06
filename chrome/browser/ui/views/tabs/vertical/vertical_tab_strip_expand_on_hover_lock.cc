// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_expand_on_hover_lock.h"

#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"

VerticalTabStripExpandOnHoverLock::VerticalTabStripExpandOnHoverLock(
    VerticalTabStripRegionView* region_view,
    ExpandOnHoverLockType lock_type)
    : lock_type_(lock_type), region_view_(region_view) {
  if (region_view_) {
    region_view_->RegisterExpandOnHoverLock(this);
  }
}

VerticalTabStripExpandOnHoverLock::~VerticalTabStripExpandOnHoverLock() {
  if (region_view_) {
    region_view_->UnregisterExpandOnHoverLock(this);
  }
}
