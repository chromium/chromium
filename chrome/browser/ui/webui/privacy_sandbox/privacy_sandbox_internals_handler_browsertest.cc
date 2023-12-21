// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
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

namespace {

using ::privacy_sandbox_internals::PrivacySandboxInternalsHandler;
using ::privacy_sandbox_internals::mojom::PageHandler;
using ::testing::AllOf;
using ::testing::Ge;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAreArray;

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

  void GetCookieContentSettingsCallback(
      const std::vector<ContentSettingPatternSource>& vec) {
    get_cookie_content_settings_cb_data_ = vec;
    waiter_.Notify();
  }

  void StringCallback(const std::string& s) {
    string_cb_data_ = s;
    waiter_.Notify();
  }

 protected:
  mojo::Remote<PageHandler> remote_;
  std::unique_ptr<PrivacySandboxInternalsHandler> handler_;

  // Notified when _any_ callback from the mojo interface is made.
  CallbackWaiter waiter_;

  std::vector<ContentSettingPatternSource> get_cookie_content_settings_cb_data_;
  std::string string_cb_data_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest,
                       GetCookieContentSettings) {
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(GURL("https://example.com"),
                             CONTENT_SETTING_ALLOW);

  remote_->GetCookieContentSettings(base::BindOnce(
      &PrivacySandboxInternalsMojoTest::GetCookieContentSettingsCallback,
      base::Unretained(this)));
  waiter_.Wait();
  EXPECT_THAT(
      get_cookie_content_settings_cb_data_,
      AllOf(
          UnorderedElementsAreArray(settings->GetCookieSettings()),
          SizeIs(Ge(1u))));  // Don't check exact size (default list may change)
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest, PatternPartsToString) {
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
}  // namespace
