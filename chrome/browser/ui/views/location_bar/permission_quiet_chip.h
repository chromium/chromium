// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_QUIET_CHIP_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_QUIET_CHIP_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/location_bar/permission_chip.h"

class Browser;
class LocationBarView;

// A less prominent version of `PermissionRequestChip`. It is used to display a
// permission request from origins with an abusive reputation, low acceptance
// rate, or if a user manually enabled "quieter messaging" in
// chrome://settings/content/notifications.
class PermissionQuietChip : public PermissionChip {
 public:
  METADATA_HEADER(PermissionQuietChip);
  PermissionQuietChip(Browser* browser,
                      permissions::PermissionPrompt::Delegate* delegate,
                      bool should_expand);
  PermissionQuietChip(const PermissionQuietChip& chip) = delete;
  PermissionQuietChip& operator=(const PermissionQuietChip& chip) = delete;
  ~PermissionQuietChip() override;

 private:
  // PermissionChip:
  views::View* CreateBubble() override;

  void RecordChipButtonPressed();
  LocationBarView* GetLocationBarView();

  raw_ptr<Browser> browser_ = nullptr;

  // The time when the chip was displayed.
  base::TimeTicks chip_shown_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PERMISSION_QUIET_CHIP_H_
