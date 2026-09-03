// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/location_bar/location_bar_override_data.h"

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace location_bar {
namespace {

class FakeLocationBar : public LocationBar {
 public:
  FakeLocationBar() : LocationBar(nullptr) {}
  ~FakeLocationBar() override = default;

  // LocationBar:
  void FocusLocation(bool is_user_initiated,
                     bool clear_focus_if_failed) override {}
  void FocusSearch() override {}
  void UpdateFocusBehavior(bool toolbar_visible) override {}
  void UpdateContentSettingsIcons() override {}
  void SaveStateToContents(content::WebContents* contents) override {}
  void Revert() override {}
  OmniboxView* GetOmniboxView() override { return nullptr; }
  OmniboxPopupView* GetOmniboxPopupView() override { return nullptr; }
  OmniboxController* GetOmniboxController() override { return nullptr; }
  bool ShouldCloseOmniboxPopup(ui::MouseEvent* event) override { return false; }
  content::WebContents* GetWebContents() override { return nullptr; }
  LocationBarModel* GetLocationBarModel() override { return nullptr; }
  std::optional<bubble_anchor_util::AnchorConfiguration> GetChipAnchor()
      override {
    return std::nullopt;
  }
  ChipController* GetChipController() override { return nullptr; }
  void AnnounceAlert(const std::u16string& announcement) override {}
  void OnChanged() override {}
  void UpdateWithoutTabRestore() override {}
  ui::TrackedElement* GetAnchorOrNull() override { return nullptr; }
  BrowserWindowInterface* GetBrowser() override { return nullptr; }
  Profile* GetProfile() override { return nullptr; }
  bool IsInitialized() const override { return true; }
  bool IsVisible() const override { return true; }
  bool IsDrawn() const override { return true; }
  bool IsFullscreen() const override { return false; }
  bool IsEditingOrEmpty() const override { return false; }
  bool IsMouseHovered() const override { return false; }
  bool IsFocusWithin() const override { return false; }
  void InvalidateLayout() override {}
  gfx::Rect Bounds() const override { return gfx::Rect(); }
  gfx::Rect BoundsInScreen() const override { return gfx::Rect(); }
  gfx::Size MinimumSize() const override { return gfx::Size(); }
  gfx::Size PreferredSize() const override { return gfx::Size(); }
  void Update(content::WebContents* contents) override {}
  void ResetTabState(content::WebContents* contents) override {}
  bool HasSecurityStateChanged() override { return false; }
  LocationBarTesting* GetLocationBarForTesting() override { return nullptr; }
};

class LocationBarOverrideDataTest : public ChromeRenderViewHostTestHarness {
 public:
  LocationBarOverrideDataTest() = default;
  ~LocationBarOverrideDataTest() override = default;
};

TEST_F(LocationBarOverrideDataTest, NoOverrideSetReturnsNullptr) {
  EXPECT_EQ(LocationBarOverrideData::FromWebContents(web_contents()), nullptr);
}

TEST_F(LocationBarOverrideDataTest, CreateForWebContentsAndGetLocationBar) {
  FakeLocationBar fake_location_bar;
  LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                &fake_location_bar);

  LocationBarOverrideData* override_data =
      LocationBarOverrideData::FromWebContents(web_contents());
  ASSERT_NE(override_data, nullptr);
  EXPECT_EQ(override_data->GetLocationBar(), &fake_location_bar);
}

TEST_F(LocationBarOverrideDataTest, CleanupOnWebContentsDestroyed) {
  std::unique_ptr<content::WebContents> test_web_contents =
      CreateTestWebContents();
  FakeLocationBar fake_location_bar;
  LocationBarOverrideData::CreateForWebContents(test_web_contents.get(),
                                                &fake_location_bar);

  EXPECT_NE(LocationBarOverrideData::FromWebContents(test_web_contents.get()),
            nullptr);

  test_web_contents.reset();
}

TEST_F(LocationBarOverrideDataTest, MultipleWebContentsHaveDistinctOverrides) {
  std::unique_ptr<content::WebContents> test_web_contents_1 =
      CreateTestWebContents();
  std::unique_ptr<content::WebContents> test_web_contents_2 =
      CreateTestWebContents();

  FakeLocationBar fake_location_bar_1;
  FakeLocationBar fake_location_bar_2;

  LocationBarOverrideData::CreateForWebContents(test_web_contents_1.get(),
                                                &fake_location_bar_1);
  LocationBarOverrideData::CreateForWebContents(test_web_contents_2.get(),
                                                &fake_location_bar_2);

  LocationBarOverrideData* override_data_1 =
      LocationBarOverrideData::FromWebContents(test_web_contents_1.get());
  LocationBarOverrideData* override_data_2 =
      LocationBarOverrideData::FromWebContents(test_web_contents_2.get());

  ASSERT_NE(override_data_1, nullptr);
  ASSERT_NE(override_data_2, nullptr);
  EXPECT_EQ(override_data_1->GetLocationBar(), &fake_location_bar_1);
  EXPECT_EQ(override_data_2->GetLocationBar(), &fake_location_bar_2);
}

TEST_F(LocationBarOverrideDataTest, NullLocationBarOverride) {
  LocationBarOverrideData::CreateForWebContents(web_contents(), nullptr);
  LocationBarOverrideData* override_data =
      LocationBarOverrideData::FromWebContents(web_contents());
  ASSERT_NE(override_data, nullptr);
  EXPECT_EQ(override_data->GetLocationBar(), nullptr);
}

TEST_F(LocationBarOverrideDataTest, CreateForWebContentsIsIdempotent) {
  FakeLocationBar fake_location_bar_1;
  FakeLocationBar fake_location_bar_2;

  LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                &fake_location_bar_1);
  LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                &fake_location_bar_2);

  LocationBarOverrideData* override_data =
      LocationBarOverrideData::FromWebContents(web_contents());
  ASSERT_NE(override_data, nullptr);
  EXPECT_EQ(override_data->GetLocationBar(), &fake_location_bar_1);
}

TEST_F(LocationBarOverrideDataTest, GetLocationBarForWebContentsNullContents) {
  EXPECT_EQ(GetLocationBarForWebContents(nullptr), nullptr);
}

TEST_F(LocationBarOverrideDataTest, GetLocationBarForWebContentsWithOverride) {
  EXPECT_EQ(GetLocationBarForWebContents(web_contents()), nullptr);

  FakeLocationBar fake_location_bar;
  LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                &fake_location_bar);

  EXPECT_EQ(GetLocationBarForWebContents(web_contents()), &fake_location_bar);
}

TEST_F(LocationBarOverrideDataTest,
       LocationBarDestroyedBeforeWebContentsReturnsNullptr) {
  auto fake_location_bar = std::make_unique<FakeLocationBar>();
  LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                fake_location_bar.get());

  LocationBarOverrideData* override_data =
      LocationBarOverrideData::FromWebContents(web_contents());
  ASSERT_NE(override_data, nullptr);
  EXPECT_EQ(override_data->GetLocationBar(), fake_location_bar.get());
  EXPECT_EQ(GetLocationBarForWebContents(web_contents()),
            fake_location_bar.get());

  // Destroy the LocationBar while WebContents and override data remain alive.
  fake_location_bar.reset();

  // The WeakPtr should have invalidated, safely returning nullptr.
  EXPECT_EQ(override_data->GetLocationBar(), nullptr);
  EXPECT_EQ(GetLocationBarForWebContents(web_contents()), nullptr);
}

TEST_F(LocationBarOverrideDataTest, FallbackToTabLocationBar) {
  tabs::MockTabInterface mock_tab;
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &mock_tab);

  // Without a browser window interface on the tab,
  // GetLocationBarForWebContents() returns nullptr.
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(nullptr));
  EXPECT_EQ(GetLocationBarForWebContents(web_contents()), nullptr);

  // When the tab has an associated browser window interface,
  // GetLocationBarForWebContents() queries the browser window features for the
  // tab's location bar.
  MockBrowserWindowInterface mock_browser;
  BrowserWindowFeatures features;
  EXPECT_CALL(mock_tab, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(&mock_browser));
  EXPECT_CALL(mock_browser, GetFeatures())
      .WillRepeatedly(testing::ReturnRef(features));
  EXPECT_EQ(GetLocationBarForWebContents(web_contents()),
            features.location_bar());

  // Attaching LocationBarOverrideData should take precedence over the fallback.
  FakeLocationBar fake_location_bar;
  LocationBarOverrideData::CreateForWebContents(web_contents(),
                                                &fake_location_bar);
  EXPECT_EQ(GetLocationBarForWebContents(web_contents()), &fake_location_bar);
}

}  // namespace
}  // namespace location_bar
