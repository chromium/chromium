// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals.mojom.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern_parser.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
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

  remote_->GetCookieSettings(
      base::BindOnce(&PrivacySandboxInternalsMojoTest::ContentSettingsCallback,
                     base::Unretained(this)));
  waiter_.Wait();
  EXPECT_THAT(
      content_settings_cb_data_,
      AllOf(
          UnorderedElementsAreArray(settings->GetCookieSettings()),
          SizeIs(Ge(1u))));  // Don't check exact size (default list may change)
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest, GetTpcdMetadataGrants) {
  ContentSettingsForOneType tpcd_metadata_grants;

  const auto primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.com");
  const auto secondary_pattern = ContentSettingsPattern::FromString("*");
  base::Value value(ContentSetting::CONTENT_SETTING_ALLOW);
  tpcd_metadata_grants.emplace_back(primary_pattern, secondary_pattern,
                                    std::move(value), std::string(), false);
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetContentSettingsFor3pcdMetadataGrants(tpcd_metadata_grants);
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
  remote_->GetTpcdHeuristicsGrants(
      base::BindOnce(&PrivacySandboxInternalsMojoTest::ContentSettingsCallback,
                     base::Unretained(this)));
  waiter_.Wait();
  EXPECT_THAT(
      content_settings_cb_data_,
      AllOf(SizeIs(Ge(1u)),
            Contains(AllOf(
                Field(&ContentSettingPatternSource::primary_pattern,
                      ContentSettingsPattern::FromString(
                          "https://[*.]google.com")),
                Field(&ContentSettingPatternSource::secondary_pattern,
                      ContentSettingsPattern::FromString(
                          "https://[*.]example.com")),
                Field(&ContentSettingPatternSource::source, "preference")))));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest, GetTpcdTrial) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetContentSettingDefaultScope(
      GURL("https://example.org"), GURL("https://example.net"),
      ContentSettingsType::TPCD_TRIAL, CONTENT_SETTING_ALLOW);
  remote_->GetTpcdTrial(
      base::BindOnce(&PrivacySandboxInternalsMojoTest::ContentSettingsCallback,
                     base::Unretained(this)));
  waiter_.Wait();
  EXPECT_THAT(
      content_settings_cb_data_,
      AllOf(SizeIs(Ge(1u)),
            Contains(AllOf(
                Field(&ContentSettingPatternSource::primary_pattern,
                      ContentSettingsPattern::FromString(
                          "https://example.org:443")),
                Field(&ContentSettingPatternSource::secondary_pattern,
                      ContentSettingsPattern::FromString(
                          "https://[*.]example.net")),
                Field(&ContentSettingPatternSource::source, "preference")))));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest, GetTopLevelTpcdTrial) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetContentSettingDefaultScope(
      GURL("https://example.org"), GURL("https://example.net"),
      ContentSettingsType::TOP_LEVEL_TPCD_TRIAL, CONTENT_SETTING_ALLOW);
  remote_->GetTopLevelTpcdTrial(
      base::BindOnce(&PrivacySandboxInternalsMojoTest::ContentSettingsCallback,
                     base::Unretained(this)));
  waiter_.Wait();
  EXPECT_THAT(
      content_settings_cb_data_,
      AllOf(SizeIs(Ge(1u)),
            Contains(AllOf(
                Field(&ContentSettingPatternSource::primary_pattern,
                      ContentSettingsPattern::FromString(
                          "https://example.org:443")),
                Field(&ContentSettingPatternSource::secondary_pattern,
                      ContentSettingsPattern::FromString("*")),
                Field(&ContentSettingPatternSource::source, "preference")))));
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
