// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_url_utils.h"

#include <memory>
#include <optional>
#include <string_view>

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class TestWebUIConfig : public content::WebUIConfig {
 public:
  TestWebUIConfig(
      std::string_view scheme,
      std::string_view host,
      std::optional<std::string_view> process_lock_host = std::nullopt)
      : WebUIConfig(scheme, host, process_lock_host) {}

  // content::WebUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    return nullptr;
  }
};

}  // namespace

TEST(WebUIURLUtilsTest, TopChromeProcessLockHostIsTopChromeWebUIURL) {
  content::ScopedWebUIConfigRegistration registration(
      std::make_unique<TestWebUIConfig>(content::kChromeUIScheme, "foo",
                                        chrome::kChromeUITopChromeDomain));

  EXPECT_TRUE(IsTopChromeWebUIURL(GURL("chrome://foo/")));
  EXPECT_TRUE(IsTopChromeWebUIURL(GURL("chrome://foo/path.html?query")));
}

TEST(WebUIURLUtilsTest, OtherProcessLockHostIsNotTopChromeWebUIURL) {
  content::ScopedWebUIConfigRegistration no_lock_host_registration(
      std::make_unique<TestWebUIConfig>(content::kChromeUIScheme,
                                        "foo.top-chrome"));
  content::ScopedWebUIConfigRegistration other_lock_host_registration(
      std::make_unique<TestWebUIConfig>(content::kChromeUIScheme,
                                        "bar.top-chrome", "bar"));

  EXPECT_FALSE(IsTopChromeWebUIURL(GURL("chrome://foo.top-chrome/")));
  EXPECT_FALSE(IsTopChromeWebUIURL(GURL("chrome://bar.top-chrome/")));
}

TEST(WebUIURLUtilsTest, UntrustedWebUIIsNotTopChromeWebUIURL) {
  content::ScopedWebUIConfigRegistration registration(
      std::make_unique<TestWebUIConfig>(content::kChromeUIUntrustedScheme,
                                        "foo.top-chrome"));

  EXPECT_FALSE(IsTopChromeWebUIURL(GURL("chrome-untrusted://foo.top-chrome/")));
}

TEST(WebUIURLUtilsTest, UnregisteredURLIsNotTopChromeWebUIURL) {
  EXPECT_FALSE(IsTopChromeWebUIURL(GURL("chrome://foo.top-chrome/")));
  EXPECT_FALSE(IsTopChromeWebUIURL(GURL("chrome://top-chrome/")));
  EXPECT_FALSE(IsTopChromeWebUIURL(GURL("https://foo.top-chrome/")));
  EXPECT_FALSE(IsTopChromeWebUIURL(GURL()));
}
