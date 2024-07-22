// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_MANAGEMENT_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_MANAGEMENT_TOOLBAR_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Browser;
class BrowserView;
class Profile;

class ManagementToolbarButton : public ToolbarButton {
  METADATA_HEADER(ManagementToolbarButton, ToolbarButton)

 public:
  explicit ManagementToolbarButton(BrowserView* browser, Profile* profile);
  ManagementToolbarButton(const ManagementToolbarButton&) = delete;
  ManagementToolbarButton& operator=(const ManagementToolbarButton&) = delete;
  ~ManagementToolbarButton() override;

  // Retrieves the latest management label and icon and stores them in
  // `management_label_` and `management_icon_` respectively.
  void UpdateManagementInfo();

  void UpdateText();

  // ToolbarButton:
  void OnThemeChanged() override;
  void UpdateIcon() override;
  void Layout(PassKey) override;
  bool ShouldPaintBorder() const override;

 private:
  void ButtonPressed();

  ui::ImageModel GetIcon() const;

  void SetManagementLabel(const std::string& management_label);
  void SetManagementIcon(const gfx::Image& management_icon);

  // Returns true if a text is set and is visible.
  bool IsLabelPresentAndVisible() const;
  // Updates the layout insets depending on whether it is a chip or a button.
  void UpdateLayoutInsets();

  std::u16string management_label_;
  gfx::Image management_icon_;
  const raw_ptr<Browser> browser_;
  const raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<ManagementToolbarButton> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_MANAGEMENT_TOOLBAR_BUTTON_H_
