// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/customize_color_scheme_mode/customize_color_scheme_mode_handler.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace {

using testing::_;

class MockClient : public customize_color_scheme_mode::mojom::
                       CustomizeColorSchemeModeClient {
 public:
  MockClient() = default;
  ~MockClient() override = default;

  mojo::PendingRemote<
      customize_color_scheme_mode::mojom::CustomizeColorSchemeModeClient>
  BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              SetColorSchemeMode,
              (customize_color_scheme_mode::mojom::ColorSchemeMode));

  mojo::Receiver<
      customize_color_scheme_mode::mojom::CustomizeColorSchemeModeClient>
      receiver_{this};
};

class MockThemeService : public ThemeService {
 public:
  MockThemeService() : ThemeService(nullptr, theme_helper_) { set_ready(); }
  MOCK_CONST_METHOD0(GetBrowserColorScheme, ThemeService::BrowserColorScheme());
  MOCK_METHOD1(SetBrowserColorScheme, void(ThemeService::BrowserColorScheme));
  MOCK_METHOD1(AddObserver, void(ThemeServiceObserver*));

 private:
  ThemeHelper theme_helper_;
};

std::unique_ptr<TestingProfile> MakeTestingProfile() {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      ThemeServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<testing::NiceMock<MockThemeService>>();
      }));
  auto profile = profile_builder.Build();
  return profile;
}
}  // namespace

class CustomizeColorSchemeModeHandlerTest : public testing::Test {
 public:
  CustomizeColorSchemeModeHandlerTest()
      : profile_(MakeTestingProfile()),
        mock_theme_service_(static_cast<MockThemeService*>(
            ThemeServiceFactory::GetForProfile(profile_.get()))) {}

  void SetUp() override {
    EXPECT_CALL(mock_theme_service(), AddObserver).Times(1);
    handler_ = std::make_unique<CustomizeColorSchemeModeHandler>(
        mock_client_.BindAndGetRemote(),
        mojo::PendingReceiver<customize_color_scheme_mode::mojom::
                                  CustomizeColorSchemeModeHandler>(),
        profile_.get());
    mock_client_.FlushForTesting();
  }

  TestingProfile& profile() { return *profile_; }
  CustomizeColorSchemeModeHandler& handler() { return *handler_; }
  MockThemeService& mock_theme_service() { return *mock_theme_service_; }

 protected:
  testing::NiceMock<MockClient> mock_client_;
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<MockThemeService> mock_theme_service_;
  std::unique_ptr<CustomizeColorSchemeModeHandler> handler_;
};

TEST_F(CustomizeColorSchemeModeHandlerTest, InitializeColorSchemeMode) {
  ON_CALL(mock_theme_service(), GetBrowserColorScheme)
      .WillByDefault(testing::Return(ThemeService::BrowserColorScheme::kLight));

  customize_color_scheme_mode::mojom::ColorSchemeMode color_scheme_mode;
  EXPECT_CALL(mock_client_, SetColorSchemeMode)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&color_scheme_mode](
              customize_color_scheme_mode::mojom::ColorSchemeMode arg) {
            color_scheme_mode = std::move(arg);
          }));

  handler().InitializeColorSchemeMode();
  mock_client_.FlushForTesting();

  EXPECT_EQ(color_scheme_mode,
            customize_color_scheme_mode::mojom::ColorSchemeMode::kLight);
}

TEST_F(CustomizeColorSchemeModeHandlerTest, OnThemeChanged) {
  ON_CALL(mock_theme_service(), GetBrowserColorScheme)
      .WillByDefault(testing::Return(ThemeService::BrowserColorScheme::kLight));

  customize_color_scheme_mode::mojom::ColorSchemeMode color_scheme_mode;
  EXPECT_CALL(mock_client_, SetColorSchemeMode)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&color_scheme_mode](
              customize_color_scheme_mode::mojom::ColorSchemeMode arg) {
            color_scheme_mode = std::move(arg);
          }));

  handler().OnThemeChanged();
  mock_client_.FlushForTesting();

  EXPECT_EQ(color_scheme_mode,
            customize_color_scheme_mode::mojom::ColorSchemeMode::kLight);
}

TEST_F(CustomizeColorSchemeModeHandlerTest, SetColorSchemeMode) {
  ThemeService::BrowserColorScheme color_scheme_mode;
  EXPECT_CALL(mock_theme_service(), SetBrowserColorScheme)
      .Times(1)
      .WillOnce(testing::Invoke(
          [&color_scheme_mode](ThemeService::BrowserColorScheme arg) {
            color_scheme_mode = std::move(arg);
          }));

  handler().SetColorSchemeMode(
      customize_color_scheme_mode::mojom::ColorSchemeMode::kLight);
  mock_client_.FlushForTesting();

  EXPECT_EQ(color_scheme_mode, ThemeService::BrowserColorScheme::kLight);
}
