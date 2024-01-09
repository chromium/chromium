// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

class OmniboxChipButton;

// UI component for activity indicators and chip button located in the omnibox.
class PermissionDashboardView : public views::View {
  METADATA_HEADER(PermissionDashboardView, views::View)

 public:
  PermissionDashboardView();
  PermissionDashboardView(const PermissionDashboardView& button) = delete;
  PermissionDashboardView& operator=(const PermissionDashboardView& button) =
      delete;
  ~PermissionDashboardView() override;

  OmniboxChipButton* GetRequestChip() { return request_chip_; }
  OmniboxChipButton* GetIndicatorChip() { return indicator_chip_; }

  // views::View.
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  raw_ptr<OmniboxChipButton> indicator_chip_;
  raw_ptr<OmniboxChipButton> request_chip_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_PERMISSION_DASHBOARD_VIEW_H_
