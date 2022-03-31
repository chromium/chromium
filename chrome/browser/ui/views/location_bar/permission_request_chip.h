// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_REQUEST_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_REQUEST_CHIP_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/location_bar/permission_chip.h"

class Browser;

// A chip view shown in the location bar to notify user about a permission
// request. Shows a permission bubble on click.
class PermissionRequestChip : public PermissionChip {
 public:
  METADATA_HEADER(PermissionRequestChip);
  // If `should_bubble_start_open` is true, a permission prompt bubble will be
  // displayed automatically after PermissionRequestChip is created.
  // `should_bubble_start_open` is evaluated based on
  // `PermissionChipGestureSensitive` and `PermissionChipRequestTypeSensitive`
  // experiments.
  PermissionRequestChip(Browser* browser,
                        permissions::PermissionPrompt::Delegate* delegate,
                        bool should_bubble_start_open);
  PermissionRequestChip(const PermissionRequestChip& chip) = delete;
  PermissionRequestChip& operator=(const PermissionRequestChip& chip) = delete;
  ~PermissionRequestChip() override;

 private:
  // PermissionChip:
  views::View* CreateBubble() override;

  void RecordChipButtonPressed();

  raw_ptr<Browser> browser_ = nullptr;

  // The time when the chip was displayed.
  base::TimeTicks chip_shown_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_REQUEST_CHIP_H_
