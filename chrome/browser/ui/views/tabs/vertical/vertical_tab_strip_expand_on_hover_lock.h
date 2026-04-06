// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_EXPAND_ON_HOVER_LOCK_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_EXPAND_ON_HOVER_LOCK_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

class VerticalTabStripRegionView;

// The type of lock to create for the vertical tab strip.
class VerticalTabStripExpandOnHoverLock : public ExpandOnHoverLock {
 public:
  explicit VerticalTabStripExpandOnHoverLock(
      VerticalTabStripRegionView* region_view,
      ExpandOnHoverLockType lock_type);
  VerticalTabStripExpandOnHoverLock(const VerticalTabStripExpandOnHoverLock&) =
      delete;
  VerticalTabStripExpandOnHoverLock& operator=(
      const VerticalTabStripExpandOnHoverLock&) = delete;
  ~VerticalTabStripExpandOnHoverLock() override;

  // Called by the parent view during its destructor.
  void ClearRegionViewOnDestruction() { region_view_ = nullptr; }

  ExpandOnHoverLockType lock_type() const { return lock_type_; }

 private:
  ExpandOnHoverLockType lock_type_;
  raw_ptr<VerticalTabStripRegionView> region_view_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_EXPAND_ON_HOVER_LOCK_H_
