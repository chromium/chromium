// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/core/ntp_load_time_data.h"

#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/skia_utils_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_source.h"
#include "ui/color/color_provider_utils.h"

namespace {

class TestColorProviderSource : public ui::ColorProviderSource {
 public:
  TestColorProviderSource() = default;
  TestColorProviderSource(const TestColorProviderSource&) = delete;
  TestColorProviderSource& operator=(const TestColorProviderSource&) = delete;
  ~TestColorProviderSource() override = default;

  const ui::ColorProvider* GetColorProvider() const override {
    return &color_provider_;
  }

  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override {
    auto key = GetColorProviderKey();
    key.color_mode = color_mode;
    key.forced_colors = forced_colors;
    ui::ColorProvider* color_provider =
        ui::ColorProviderManager::Get().GetColorProviderFor(key);
    CHECK(color_provider);
    return ui::CreateRendererColorMap(*color_provider);
  }

  void SetColor(ui::ColorId id, SkColor color) {
    color_provider_.SetColorForTesting(id, color);
    NotifyColorProviderChanged();
  }

 protected:
  ui::ColorProviderKey GetColorProviderKey() const override {
    return ui::ColorProviderKey();
  }

 private:
  ui::ColorProvider color_provider_;
};

}  // namespace

class NtpLoadTimeDataTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents()->SetColorProviderSource(&color_provider_source_);
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)}};
  }

  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  TestColorProviderSource color_provider_source_;
};

TEST_F(NtpLoadTimeDataTest, BuildLoadTimeDataExcludesDynamicEntries) {
  base::DictValue dict =
      ntp::BuildLoadTimeData(profile(), /*session_allows_drag_and_drop=*/false);
  EXPECT_NE(dict.FindBool("actionChipsEnabled"), std::nullopt);
  EXPECT_NE(dict.FindString("undoDescription"), nullptr);
  // Navigation- and theme-dependent entries are built separately.
  EXPECT_EQ(dict.FindBool("modulesEnabled"), std::nullopt);
  EXPECT_EQ(dict.FindString("browserPromoType"), nullptr);
  EXPECT_EQ(dict.FindString("backgroundColor"), nullptr);
  EXPECT_EQ(dict.FindString("backgroundImageUrl"), nullptr);
}

TEST_F(NtpLoadTimeDataTest, BuildNavigationLoadTimeData) {
  base::DictValue update = ntp::BuildNavigationLoadTimeData(
      profile(), web_contents(), /*module_id_details=*/{});
  EXPECT_EQ(update.FindBool("modulesEnabled"), false);
  ASSERT_NE(update.FindString("browserPromoType"), nullptr);
  // `web_contents()` is not in a browser tab, so promos are disabled.
  EXPECT_EQ(*update.FindString("browserPromoType"), "disabled");
}

TEST_F(NtpLoadTimeDataTest, BuildCustomBackgroundImageLoadTimeData) {
  base::DictValue update =
      ntp::BuildCustomBackgroundImageLoadTimeData(profile());
  ASSERT_NE(update.FindString("backgroundImageUrl"), nullptr);
  EXPECT_EQ(*update.FindString("backgroundImageUrl"), "");
}

TEST_F(NtpLoadTimeDataTest, BuildBackgroundColorLoadTimeData) {
  color_provider_source_.SetColor(kColorNewTabPageBackground,
                                  SkColorSetRGB(0xAA, 0xBB, 0xCC));
  base::DictValue update =
      ntp::BuildBackgroundColorLoadTimeData(web_contents());
  ASSERT_NE(update.FindString("backgroundColor"), nullptr);
  EXPECT_EQ(*update.FindString("backgroundColor"),
            skia::SkColorToHexString(SkColorSetRGB(0xAA, 0xBB, 0xCC)));
}
