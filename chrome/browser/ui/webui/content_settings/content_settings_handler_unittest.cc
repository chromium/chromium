// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/content_settings/content_settings_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/test_future.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/ui/webui/content_settings/content_settings_internals.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings_internals {
namespace {

using ::content_settings_internals::mojom::PageHandler;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAreArray;

class ContentSettingsHandlerTest : public testing::Test {
 public:
  ContentSettingsHandlerTest()
      : handler_(&profile_, remote_.BindNewPipeAndPassReceiver()) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  mojo::Remote<PageHandler> remote_;
  ContentSettingsHandler handler_;
};

TEST_F(ContentSettingsHandlerTest, GetCookieSettings) {
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(&profile_).get();
  settings->SetCookieSetting(GURL("https://example.com"),
                             CONTENT_SETTING_ALLOW);

  base::test::TestFuture<const std::vector<ContentSettingPatternSource>&>
      future;
  remote_->ReadContentSettings(ContentSettingsType::COOKIES,
                               future.GetCallback());
  auto& content_settings_cb_data = future.Get();
  EXPECT_THAT(content_settings_cb_data,
              AllOf(UnorderedElementsAreArray(settings->GetCookieSettings()),
                    SizeIs(Ge(1u))));
}

class ContentSettingsHandlerTypesTest
    : public ContentSettingsHandlerTest,
      public testing::WithParamInterface<int> {
 public:
  ContentSettingsHandlerTypesTest() {
    EXPECT_THAT(ContentSettingsType::DEFAULT,
                Eq(ContentSettingsType::kMinValue))
        << "This test depends on kMinValue being equal to the DEFAULT content "
           "setting.";
  }
};

INSTANTIATE_TEST_SUITE_P(
    ContentSettingsMojoTests,
    ContentSettingsHandlerTypesTest,
    testing::Range(static_cast<int>(ContentSettingsType::kMinValue),
                   static_cast<int>(ContentSettingsType::kMaxValue)));

TEST_P(ContentSettingsHandlerTypesTest, ReadContentSettingsEmpty) {
  ContentSettingsType type = static_cast<ContentSettingsType>(GetParam());
  EXPECT_TRUE(IsKnownEnumValue(type));
  base::test::TestFuture<const std::vector<ContentSettingPatternSource>&>
      future;
  remote_->ReadContentSettings(type, future.GetCallback());
  auto& content_settings_cb_data = future.Get();
  EXPECT_THAT(content_settings_cb_data, SizeIs(Ge(0u)));
}

TEST_F(ContentSettingsHandlerTest, ContentSettingsPatternToString) {
  for (const std::string& regex :
       {"[*.]example.com", "http://example.net", "example.org"}) {
    ContentSettingsPattern pattern = ContentSettingsPattern::FromString(regex);
    base::test::TestFuture<const std::string&> future;
    remote_->ContentSettingsPatternToString(pattern, future.GetCallback());
    auto& string_cb_data = future.Get();
    EXPECT_THAT(string_cb_data, StrEq(pattern.ToString()));
  }
}

TEST_F(ContentSettingsHandlerTest, StringToContentSettingsPattern) {
  base::test::TestFuture<const ContentSettingsPattern&> future;
  for (const std::string& regex :
       {"[*.]example.com", "http://example.net", "example.org"}) {
    remote_->StringToContentSettingsPattern(regex, future.GetCallback());
    ContentSettingsPattern expected_pattern =
        ContentSettingsPattern::FromString(regex);
    auto& content_settings_pattern_cb_data = future.Get();
    EXPECT_THAT(content_settings_pattern_cb_data, Eq(expected_pattern));
    future.Clear();
  }
}

}  // namespace
}  // namespace content_settings_internals
