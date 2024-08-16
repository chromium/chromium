// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_CHIP_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_CHIP_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;
class IntentPickerTabHelper;

// A chip-style button which allows opening the current URL in an installed app.
class IntentChipButton : public OmniboxChipButton {
  METADATA_HEADER(IntentChipButton, OmniboxChipButton)

 public:
  // TODO(crbug.com/40821394): Consider creating a more appropriate Delegate
  // interface.
  explicit IntentChipButton(Browser* browser,
                            PageActionIconView::Delegate* delegate);
  IntentChipButton(const IntentChipButton&) = delete;
  IntentChipButton& operator=(const IntentChipButton&) = delete;
  ~IntentChipButton() override;

  void Update();
  ui::ImageModel GetAppIconForTesting() const;

 private:
  bool GetShowChip() const;
  bool GetChipExpanded() const;
  ui::ImageModel GetAppIcon() const;
  void HandlePressed();

  IntentPickerTabHelper* GetTabHelper() const;

  // OmniboxChipButton:
  ui::ImageModel GetIconImageModel() const override;
  const gfx::VectorIcon& GetIcon() const override;
  ui::ColorId GetForegroundColorId() const override;
  ui::ColorId GetBackgroundColorId() const override;

  const raw_ptr<Browser> browser_;
  const raw_ptr<PageActionIconView::Delegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_INTENT_CHIP_BUTTON_H_
