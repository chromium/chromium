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

using ::privacy_sandbox_internals::mojom::PageHandler;
using ::testing::AllOf;
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

// Given an implementation `Handler` of a mojo handler for `Interface`, will
// wire up a pipe with this object acting as the remote (client) side.
template <typename Handler, typename Interface>
class MojoPiping {
 public:
  explicit MojoPiping(Handler handler)
      : handler_(std::move(handler)),
        // Connect both ends to the mojo pipe.  The `Remote` is the client end
        // and the `Receiver` is the end connected to the handler.
        remote_(mojo::PendingRemote<Interface>(std::move(pipe_.handle0), 0)),
        receiver_(&handler_,
                  mojo::PendingReceiver<Interface>(std::move(pipe_.handle1))) {}

  // For this test we only care about access to the `Remote` object since the
  // test is acting as a client.
  typename Interface::Proxy_* get() const { return remote_.get(); }
  typename Interface::Proxy_* operator->() const { return get(); }
  typename Interface::Proxy_& operator*() const { return *get(); }

 private:
  mojo::MessagePipe pipe_;
  Handler handler_;
  mojo::Remote<PageHandler> remote_;
  mojo::Receiver<PageHandler> receiver_;
};

class PrivacySandboxInternalsMojoTest : public InProcessBrowserTest {
 public:
  PrivacySandboxInternalsMojoTest() = default;

  MojoPiping<PrivacySandboxInternalsHandler, PageHandler> MakeMojoPiping() {
    return MojoPiping<PrivacySandboxInternalsHandler, PageHandler>(
        PrivacySandboxInternalsHandler(browser()->profile()));
  }

  void GetCookieContentSettingsCallback(
      const std::vector<ContentSettingPatternSource>& vec) {
    get_cookie_content_settings_cb_data_ = vec;
    waiter_.Notify();
  }

  void ContentSettingsPatternToStringCallback(const std::string& s) {
    content_settings_pattern_to_string_cb_data_ = s;
    waiter_.Notify();
  }

 protected:
  // Notified when _any_ callback from the mojo interface is made.
  CallbackWaiter waiter_;

  std::vector<ContentSettingPatternSource> get_cookie_content_settings_cb_data_;
  std::string content_settings_pattern_to_string_cb_data_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest,
                       GetCookieContentSettings) {
  auto mojo = MakeMojoPiping();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetCookieSetting(GURL("https://example.com"),
                             CONTENT_SETTING_ALLOW);

  mojo->GetCookieContentSettings(base::BindOnce(
      &PrivacySandboxInternalsMojoTest::GetCookieContentSettingsCallback,
      base::Unretained(this)));
  waiter_.Wait();
  EXPECT_THAT(get_cookie_content_settings_cb_data_,
              AllOf(UnorderedElementsAreArray(settings->GetCookieSettings()),
                    SizeIs(2)));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxInternalsMojoTest, PatternPartsToString) {
  auto mojo = MakeMojoPiping();

  for (const std::string& regex :
       {"[*.]example.com", "http://example.net", "example.org"}) {
    ContentSettingsPattern pattern = ContentSettingsPattern::FromString(regex);
    mojo->ContentSettingsPatternToString(
        pattern, base::BindOnce(&PrivacySandboxInternalsMojoTest::
                                    ContentSettingsPatternToStringCallback,
                                base::Unretained(this)));
    waiter_.Wait();
    waiter_.Reset();
    EXPECT_THAT(content_settings_pattern_to_string_cb_data_,
                StrEq(pattern.ToString()));
  }
}
}  // namespace
