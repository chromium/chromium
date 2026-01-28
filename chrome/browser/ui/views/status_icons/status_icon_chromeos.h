// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_CHROMEOS_H_

#include <optional>

#include "chrome/browser/status_icons/status_icon.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/image/image_skia.h"

class StatusIconChromeOS : public StatusIcon, public display::DisplayObserver {
 public:
  explicit StatusIconChromeOS(int64_t icon_id);

  StatusIconChromeOS(const StatusIconChromeOS&) = delete;
  StatusIconChromeOS& operator=(const StatusIconChromeOS&) = delete;

  ~StatusIconChromeOS() override;

  void Initialize();

  void OnClick();

  // StatusIcon:
  void SetImage(const gfx::ImageSkia& image) override;
  void SetToolTip(const std::u16string& tool_tip) override;
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnWillRemoveDisplays(const display::Displays& removed_displays) override;

 protected:
  void UpdatePlatformContextMenu(StatusIconMenuModel* model) override;

 private:
  void AddStatusIconForDisplay(int64_t display_id);

  const int64_t id_;
  std::optional<std::u16string> tool_tip_;
  std::optional<gfx::ImageSkia> image_;

  bool initialized_ = false;

  std::optional<display::ScopedDisplayObserver> display_observer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_CHROMEOS_H_
