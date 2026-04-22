// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_INTERFACE_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_INTERFACE_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/bubble/bubble_anchor.h"

class PermissionChipInterface;

// An abstract interface for manipulating the permission dashboard, which
// contains the activity indicator chip and the permission request chip.
class PermissionDashboardInterface {
 public:
  virtual ~PermissionDashboardInterface() = default;

  virtual void SetVisible(bool visible) = 0;
  virtual bool GetVisible() const = 0;

  // Return the chip that is used to display a permission request and blockade
  // indicator.
  virtual PermissionChipInterface* GetRequestChip() = 0;

  // Return the chip that is used to display in-use activity indicators.
  virtual PermissionChipInterface* GetIndicatorChip() = 0;

  virtual views::BubbleAnchor GetAnchor() = 0;
};
#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_INTERFACE_H_
