// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_TEST_LOCATION_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_TEST_LOCATION_BAR_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

class CommandUpdater;
class LocationBarModel;
class OmniboxViewViews;
class Profile;

class TestLocationBar : public LocationBar {
 public:
  explicit TestLocationBar(CommandUpdater* command_updater = nullptr,
                           LocationBarModel* location_bar_model = nullptr);
  TestLocationBar(const TestLocationBar&) = delete;
  TestLocationBar& operator=(const TestLocationBar&) = delete;
  ~TestLocationBar() override;

  void set_omnibox_view(OmniboxViewViews* view) { omnibox_view_ = view; }
  void set_profile(Profile* profile) { profile_ = profile; }
  void set_bounds(const gfx::Rect& bounds) { bounds_ = bounds; }
  void set_bounds_in_screen(const gfx::Rect& bounds_in_screen) {
    bounds_in_screen_ = bounds_in_screen;
  }

  // LocationBar:
  void FocusLocation(bool select_all, bool clear_focus_if_failed) override;
  void FocusSearch() override;
  void UpdateFocusBehavior(bool toolbar_visible) override;
  void UpdateContentSettingsIcons() override;
  void SaveStateToContents(content::WebContents* contents) override;
  void Revert() override;
  OmniboxView* GetOmniboxView() override;
  OmniboxPopupView* GetOmniboxPopupView() override;
  OmniboxController* GetOmniboxController() override;
  bool ShouldCloseOmniboxPopup(ui::MouseEvent* event) override;
  ChipController* GetChipController() override;
  LocationBarTesting* GetLocationBarForTesting() override;
  LocationBarModel* GetLocationBarModel() override;
  content::WebContents* GetWebContents() override;
  std::optional<bubble_anchor_util::AnchorConfiguration> GetChipAnchor()
      override;
  void AnnounceAlert(const std::u16string& announcement) override;
  void OnChanged() override;
  void UpdateWithoutTabRestore() override;
  ui::TrackedElement* GetAnchorOrNull() override;
  BrowserWindowInterface* GetBrowser() override;
  Profile* GetProfile() override;
  bool IsInitialized() const override;
  bool IsVisible() const override;
  bool IsDrawn() const override;
  bool IsFullscreen() const override;
  bool IsEditingOrEmpty() const override;
  bool IsMouseHovered() const override;
  bool IsFocusWithin() const override;
  void InvalidateLayout() override;
  gfx::Rect Bounds() const override;
  gfx::Rect BoundsInScreen() const override;
  gfx::Size MinimumSize() const override;
  gfx::Size PreferredSize() const override;
  void Update(content::WebContents* contents) override;
  void ResetTabState(content::WebContents* contents) override;
  bool HasSecurityStateChanged() override;

 private:
  raw_ptr<LocationBarModel> location_bar_model_ = nullptr;
  raw_ptr<OmniboxViewViews> omnibox_view_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;
  gfx::Rect bounds_{0, 0, 800, 34};
  gfx::Rect bounds_in_screen_{100, 50, 800, 34};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_TEST_LOCATION_BAR_H_
