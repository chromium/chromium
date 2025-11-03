// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"

#include "base/test/test_future.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern_parser.h"
#include "components/tpcd/metadata/browser/parser.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox_internals {
namespace {
using ::privacy_sandbox_internals::mojom::PageHandler;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Ge;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAreArray;

class PrivacySandboxInternalsMojoTest : public InProcessBrowserTest {
 public:
  PrivacySandboxInternalsMojoTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    handler_ = std::make_unique<PrivacySandboxInternalsHandler>(
        browser()->profile(), remote_.BindNewPipeAndPassReceiver());
  }

 protected:
  mojo::Remote<PageHandler> remote_;
  std::unique_ptr<PrivacySandboxInternalsHandler> handler_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest, GetCookieSettings) {
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(GURL("https://example.com"),
                             CONTENT_SETTING_ALLOW);

  base::test::TestFuture<const std::vector<ContentSettingPatternSource>&>
      future;
  remote_->ReadContentSettings(ContentSettingsType::COOKIES,
                               future.GetCallback());
  auto& content_settings_cb_data_ = future.Get();
  EXPECT_THAT(
      content_settings_cb_data_,
      AllOf(
          UnorderedElementsAreArray(settings->GetCookieSettings()),
          SizeIs(Ge(1u))));  // Don't check exact size (default list may change)
}

// TODO(crbug.com/41490688): Once ConvertGenerator<T>() is provided by
// the version of googletest used by Chromium we can type the test param.
class PrivacySandboxInternalsContentSettingsMojoTest
    : public PrivacySandboxInternalsMojoTest,
      public testing::WithParamInterface<int> {
 public:
  PrivacySandboxInternalsContentSettingsMojoTest() {
    EXPECT_THAT(ContentSettingsType::DEFAULT,
                Eq(ContentSettingsType::kMinValue))
        << "This test depends on kMinValue being equal to the DEFAULT content "
           "setting.";
  }
};

INSTANTIATE_TEST_SUITE_P(
    ContentSettingsMojoTests,
    PrivacySandboxInternalsContentSettingsMojoTest,
    testing::Range(static_cast<int>(ContentSettingsType::kMinValue),
                   static_cast<int>(ContentSettingsType::kMaxValue)));

IN_PROC_BROWSER_TEST_P(PrivacySandboxInternalsContentSettingsMojoTest,
                       ReadContentSettingsEmpty) {
  ContentSettingsType type = static_cast<ContentSettingsType>(GetParam());
  LOG(INFO) << "Testing for type " << type;
  EXPECT_TRUE(IsKnownEnumValue(type));
  base::test::TestFuture<const std::vector<ContentSettingPatternSource>&>
      future;
  remote_->ReadContentSettings(type, future.GetCallback());
  auto& content_settings_cb_data_ = future.Get();
  // May or may not have a default value, but the read should succeed either
  // way.
  EXPECT_THAT(content_settings_cb_data_, SizeIs(Ge(0u)));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest, GetTpcdMetadataGrants) {
  const auto primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  const auto secondary_pattern = ContentSettingsPattern::FromString("*");

  tpcd::metadata::Metadata metadata;
  tpcd::metadata::helpers::AddEntryToMetadata(
      metadata, primary_pattern.ToString(), secondary_pattern.ToString(),
      tpcd::metadata::Parser::kSourceTest, /*dtrp=*/0);
  EXPECT_EQ(metadata.metadata_entries_size(), 1);

  auto* tpcd_metadata_parser = tpcd::metadata::Parser::GetInstance();
  tpcd_metadata_parser->ParseMetadata(metadata.SerializeAsString());

  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();

  base::test::TestFuture<const std::vector<ContentSettingPatternSource>&>
      future;
  // TODO: TPCD_METADATA_GRANTS are special and don't show up if read with the
  // regular method.
  remote_->GetTpcdMetadataGrants(future.GetCallback());
  auto& content_settings_cb_data_ = future.Get();
  EXPECT_THAT(content_settings_cb_data_,
              AllOf(SizeIs(1), UnorderedElementsAreArray(
                                   settings->GetTpcdMetadataGrants())));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest,
                       GetTpcdHeuristicGrants) {
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetTemporaryCookieGrantForHeuristic(
      GURL("https://accounts.google.com"), GURL("https://example.com"),
      base::Microseconds(1e10));
  base::test::TestFuture<const std::vector<ContentSettingPatternSource>&>
      future;
  remote_->ReadContentSettings(ContentSettingsType::TPCD_HEURISTICS_GRANTS,
                               future.GetCallback());
  auto& content_settings_cb_data_ = future.Get();
  EXPECT_THAT(
      content_settings_cb_data_,
      AllOf(SizeIs(Ge(1u)),
            Contains(
                AllOf(Field(&ContentSettingPatternSource::primary_pattern,
                            ContentSettingsPattern::FromString(
                                "https://[*.]google.com")),
                      Field(&ContentSettingPatternSource::secondary_pattern,
                            ContentSettingsPattern::FromString(
                                "https://[*.]example.com")),
                      Field(&ContentSettingPatternSource::source,
                            content_settings::ProviderType::kPrefProvider)))));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest,
                       ContentSettingsPatternToString) {
  for (const std::string& regex :
       {"[*.]example.com", "http://example.net", "example.org"}) {
    ContentSettingsPattern pattern = ContentSettingsPattern::FromString(regex);
    base::test::TestFuture<const std::string&> future;
    remote_->ContentSettingsPatternToString(pattern, future.GetCallback());
    auto& string_cb_data_ = future.Get();
    EXPECT_THAT(string_cb_data_, StrEq(pattern.ToString()));
  }
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest,
                       StringToContentSettingsPattern) {
  base::test::TestFuture<const ContentSettingsPattern&> future;
  for (const std::string& regex :
       {"[*.]example.com", "http://example.net", "example.org"}) {
    remote_->StringToContentSettingsPattern(regex, future.GetCallback());
    ContentSettingsPattern expected_pattern =
        ContentSettingsPattern::FromString(regex);
    auto& content_settings_pattern_cb_data_ = future.Get();
    EXPECT_THAT(content_settings_pattern_cb_data_, Eq(expected_pattern));
    future.Clear();
  }
}

}  // namespace
}  // namespace privacy_sandbox_internals
