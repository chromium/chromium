// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
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
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAreArray;

static const char kPrefName[] =
    "privacy_sandbox.topics_consent.last_update_time";

// Helper to aid in waiting for mojo callbacks to happen.
class CallbackWaiter {
 public:
  void Notify() {
    waiting_ = false;
    if (runner_.get()) {
      runner_->Quit();
    }
  }

  void Wait() {
    if (waiting_) {
      runner_ = std::make_unique<base::RunLoop>();
      runner_->Run();
      runner_.reset();
    }
  }

  void Reset() {
    waiting_ = true;
    if (runner_.get()) {
      runner_->Quit();
    }
    runner_.reset();
  }

 private:
  bool waiting_{true};
  std::unique_ptr<base::RunLoop> runner_;
};

class PrivacySandboxInternalsMojoTest : public InProcessBrowserTest {
 public:
  PrivacySandboxInternalsMojoTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    handler_ = std::make_unique<PrivacySandboxInternalsHandler>(
        browser()->profile(), remote_.BindNewPipeAndPassReceiver());
  }

  void ContentSettingsCallback(
      const std::vector<ContentSettingPatternSource>& vec) {
    content_settings_cb_data_ = vec;
    waiter_.Notify();
  }

  void StringCallback(const std::string& s) {
    string_cb_data_ = s;
    waiter_.Notify();
  }

  void ValueCallback(base::Value v) {
    value_cb_data_ = std::move(v);
    waiter_.Notify();
  }

  void ContentSettingsPatternCallback(const ContentSettingsPattern& pattern) {
    content_settings_pattern_cb_data_ = pattern;
    waiter_.Notify();
  }

 protected:
  mojo::Remote<PageHandler> remote_;
  std::unique_ptr<PrivacySandboxInternalsHandler> handler_;

  // Notified when _any_ callback from the mojo interface is made.
  CallbackWaiter waiter_;

  std::vector<ContentSettingPatternSource> content_settings_cb_data_;
  std::string string_cb_data_;
  base::Value value_cb_data_;
  ContentSettingsPattern content_settings_pattern_cb_data_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest, ReadPref) {
  browser()->profile()->GetPrefs()->SetString(kPrefName,
                                              "this is a test pref string!");
  remote_->ReadPref(
      kPrefName, base::BindOnce(&PrivacySandboxInternalsMojoTest::ValueCallback,
                                base::Unretained(this)));
  waiter_.Wait();
  waiter_.Reset();
  EXPECT_THAT(value_cb_data_, Property(&base::Value::GetString,
                                       StrEq("this is a test pref string!")));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest, ReadPref_NonExistant) {
  remote_->ReadPref(
      "foo", base::BindOnce(&PrivacySandboxInternalsMojoTest::ValueCallback,
                            base::Unretained(this)));
  waiter_.Wait();
  waiter_.Reset();
  EXPECT_THAT(value_cb_data_, Property(&base::Value::is_none, Eq(true)));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest, GetCookieSettings) {
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(GURL("https://example.com"),
                             CONTENT_SETTING_ALLOW);

  remote_->ReadContentSettings(
      ContentSettingsType::COOKIES,
      base::BindOnce(&PrivacySandboxInternalsMojoTest::ContentSettingsCallback,
                     base::Unretained(this)));
  waiter_.Wait();
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
  remote_->ReadContentSettings(
      type,
      base::BindOnce(&PrivacySandboxInternalsMojoTest::ContentSettingsCallback,
                     base::Unretained(this)));
  waiter_.Wait();
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

  // TODO: TPCD_METADATA_GRANTS are special and don't show up if read with the
  // regular method.
  remote_->GetTpcdMetadataGrants(
      base::BindOnce(&PrivacySandboxInternalsMojoTest::ContentSettingsCallback,
                     base::Unretained(this)));
  waiter_.Wait();
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
  remote_->ReadContentSettings(
      ContentSettingsType::TPCD_HEURISTICS_GRANTS,
      base::BindOnce(&PrivacySandboxInternalsMojoTest::ContentSettingsCallback,
                     base::Unretained(this)));
  waiter_.Wait();
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

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest, GetTpcdTrial) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetContentSettingDefaultScope(
      GURL("https://example.org"), GURL("https://example.net"),
      ContentSettingsType::TPCD_TRIAL, CONTENT_SETTING_ALLOW);
  remote_->ReadContentSettings(
      ContentSettingsType::TPCD_TRIAL,
      base::BindOnce(&PrivacySandboxInternalsMojoTest::ContentSettingsCallback,
                     base::Unretained(this)));
  waiter_.Wait();
  EXPECT_THAT(
      content_settings_cb_data_,
      AllOf(SizeIs(Ge(1u)),
            Contains(
                AllOf(Field(&ContentSettingPatternSource::primary_pattern,
                            ContentSettingsPattern::FromString(
                                "https://example.org:443")),
                      Field(&ContentSettingPatternSource::secondary_pattern,
                            ContentSettingsPattern::FromString(
                                "https://[*.]example.net")),
                      Field(&ContentSettingPatternSource::source,
                            content_settings::ProviderType::kPrefProvider)))));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest, GetTopLevelTpcdTrial) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetContentSettingDefaultScope(
      GURL("https://example.org"), GURL("https://example.net"),
      ContentSettingsType::TOP_LEVEL_TPCD_TRIAL, CONTENT_SETTING_ALLOW);
  remote_->ReadContentSettings(
      ContentSettingsType::TOP_LEVEL_TPCD_TRIAL,
      base::BindOnce(&PrivacySandboxInternalsMojoTest::ContentSettingsCallback,
                     base::Unretained(this)));
  waiter_.Wait();
  EXPECT_THAT(
      content_settings_cb_data_,
      AllOf(SizeIs(Ge(1u)),
            Contains(
                AllOf(Field(&ContentSettingPatternSource::primary_pattern,
                            ContentSettingsPattern::FromString(
                                "https://example.org:443")),
                      Field(&ContentSettingPatternSource::secondary_pattern,
                            ContentSettingsPattern::FromString("*")),
                      Field(&ContentSettingPatternSource::source,
                            content_settings::ProviderType::kPrefProvider)))));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest,
                       ContentSettingsPatternToString) {
  for (const std::string& regex :
       {"[*.]example.com", "http://example.net", "example.org"}) {
    ContentSettingsPattern pattern = ContentSettingsPattern::FromString(regex);
    remote_->ContentSettingsPatternToString(
        pattern,
        base::BindOnce(&PrivacySandboxInternalsMojoTest::StringCallback,
                       base::Unretained(this)));
    waiter_.Wait();
    waiter_.Reset();
    EXPECT_THAT(string_cb_data_, StrEq(pattern.ToString()));
  }
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest,
                       StringToContentSettingsPattern) {
  for (const std::string& regex :
       {"[*.]example.com", "http://example.net", "example.org"}) {
    remote_->StringToContentSettingsPattern(
        regex,
        base::BindOnce(
            &PrivacySandboxInternalsMojoTest::ContentSettingsPatternCallback,
            base::Unretained(this)));
    ContentSettingsPattern expected_pattern =
        ContentSettingsPattern::FromString(regex);
    waiter_.Wait();
    waiter_.Reset();
    EXPECT_THAT(content_settings_pattern_cb_data_, Eq(expected_pattern));
  }
}

}  // namespace
}  // namespace privacy_sandbox_internals
