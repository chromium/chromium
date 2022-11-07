// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SITE_SETTINGS_EXPAND_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SITE_SETTINGS_EXPAND_BUTTON_H_

#include "chrome/browser/ui/views/controls/hover_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class SiteSettingsExpandButton : public HoverButton {
 public:
  METADATA_HEADER(SiteSettingsExpandButton);

  explicit SiteSettingsExpandButton(PressedCallback callback);
  SiteSettingsExpandButton(const SiteSettingsExpandButton&) = delete;
  SiteSettingsExpandButton operator=(const SiteSettingsExpandButton&) = delete;
  ~SiteSettingsExpandButton() override;

  void SetIcon(bool expand_state);

 private:
  raw_ptr<views::ImageView> icon_ = nullptr;
};

BEGIN_VIEW_BUILDER(/* no export */, SiteSettingsExpandButton, HoverButton)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, SiteSettingsExpandButton)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SITE_SETTINGS_EXPAND_BUTTON_H_
