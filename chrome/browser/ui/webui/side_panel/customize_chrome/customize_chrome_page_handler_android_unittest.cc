// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_source.h"
#include "ui/color/color_provider_utils.h"
#include "ui/native_theme/native_theme.h"

namespace {

using testing::Mock;

class MockPage : public side_panel::mojom::CustomizeChromePage {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<side_panel::mojom::CustomizeChromePage>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              SetModulesSettings,
              (std::vector<side_panel::mojom::ModuleSettingsPtr>, bool, bool),
              (override));
  MOCK_METHOD(void,
              SetMostVisitedSettings,
              (const std::vector<ntp_tiles::TileType>&,
               bool,
               bool,
               const std::vector<ntp_tiles::TileType>&),
              (override));
  MOCK_METHOD(void, SetTheme, (side_panel::mojom::ThemePtr), (override));
  MOCK_METHOD(void, SetThemeEditable, (bool), (override));
  MOCK_METHOD(void, ScrollToSection, (CustomizeChromeSection), (override));
  MOCK_METHOD(void,
              AttachedTabStateUpdated,
              (side_panel::mojom::NewTabPageType),
              (override));
  MOCK_METHOD(void,
              NtpManagedByNameUpdated,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, SetToolsSettings, (bool), (override));
  MOCK_METHOD(void,
              SetFooterSettings,
              (bool, bool, side_panel::mojom::ManagementNoticeStatePtr),
              (override));

  mojo::Receiver<side_panel::mojom::CustomizeChromePage> receiver_{this};
};

class FakeColorProviderSource : public ui::ColorProviderSource {
 public:
  FakeColorProviderSource() = default;
  ~FakeColorProviderSource() override = default;

  const ui::ColorProvider* GetColorProvider() const override {
    return &color_provider_;
  }

  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override {
    return ui::CreateRendererColorMap(color_provider_);
  }

  void SetColor(ui::ColorId id, SkColor color) {
    color_provider_.SetColorForTesting(id, color);
  }

 protected:
  ui::ColorProviderKey GetColorProviderKey() const override {
    return ui::ColorProviderKey();
  }

 private:
  ui::ColorProvider color_provider_;
};

class CustomizeChromePageHandlerAndroidTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    theme_service_ = ThemeServiceFactory::GetForProfile(profile_.get());
    theme_service_->SetUserColor(SkColorSetRGB(0xFF, 0x80, 0x00));
    web_contents_ = web_contents_factory_.CreateWebContents(profile_.get());
    web_contents_->SetColorProviderSource(&color_provider_source_);

    handler_ = std::make_unique<CustomizeChromePageHandler>(
        mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>(),
        mock_page_.BindAndGetRemote(),
        NtpCustomBackgroundServiceFactory::GetForProfile(profile_.get()),
        web_contents_, std::vector<ntp::ModuleIdDetail>(), base::DoNothing());
    mock_page_.FlushForTesting();
    Mock::VerifyAndClearExpectations(&mock_page_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<ThemeService> theme_service_ = nullptr;
  FakeColorProviderSource color_provider_source_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  testing::NiceMock<MockPage> mock_page_;
  std::unique_ptr<CustomizeChromePageHandler> handler_;
};

TEST_F(CustomizeChromePageHandlerAndroidTest,
       OnColorProviderChangedUpdatesDeduplicatesAndResetsOnUpdateTheme) {
  color_provider_source_.SetColor(ui::kColorSysInversePrimary,
                                  SkColorSetRGB(0x4F, 0x36, 0x25));

  // ColorProvider change sends the updated theme.
  side_panel::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce([&theme](side_panel::mojom::ThemePtr arg) {
        theme = std::move(arg);
      });
  color_provider_source_.NotifyColorProviderChanged();
  mock_page_.FlushForTesting();
  ASSERT_TRUE(theme);
  EXPECT_EQ(SkColorSetRGB(0x4F, 0x36, 0x25), theme->background_color);
  EXPECT_EQ(SkColorSetRGB(0x4F, 0x36, 0x25), theme->foreground_color);
  Mock::VerifyAndClearExpectations(&mock_page_);

  // Duplicate ThemeService / NativeTheme observer updates are suppressed.
  EXPECT_CALL(mock_page_, SetTheme).Times(0);
  theme_service_->SetUserColor(SkColorSetRGB(0xFF, 0x80, 0x00));
  ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();
  mock_page_.FlushForTesting();
  Mock::VerifyAndClearExpectations(&mock_page_);

  // Explicit Mojo UpdateTheme() clears the cached theme and always sends.
  EXPECT_CALL(mock_page_, SetTheme).Times(1);
  handler_->UpdateTheme();
  mock_page_.FlushForTesting();
  Mock::VerifyAndClearExpectations(&mock_page_);

  // A subsequent ColorProvider change with different colors sends again.
  theme.reset();
  color_provider_source_.SetColor(ui::kColorSysInversePrimary,
                                  SkColorSetRGB(0x11, 0x22, 0x33));
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce([&theme](side_panel::mojom::ThemePtr arg) {
        theme = std::move(arg);
      });
  color_provider_source_.NotifyColorProviderChanged();
  mock_page_.FlushForTesting();
  ASSERT_TRUE(theme);
  EXPECT_EQ(SkColorSetRGB(0x11, 0x22, 0x33), theme->background_color);
}

}  // namespace
