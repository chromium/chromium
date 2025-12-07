// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_urls/chrome_urls_handler.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/webui/chrome_urls/mojom/chrome_urls.mojom.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/webui/file_manager/url_constants.h"
#include "ash/webui/sanitize_ui/url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

using testing::_;

namespace chrome_urls {

namespace {

// Test config class, to add to the WebUIConfigMap for testing.
class TestWebUIConfig : public content::WebUIConfig {
 public:
  TestWebUIConfig(const std::string& scheme,
                  const std::string& host,
                  bool enabled)
      : content::WebUIConfig(scheme, host), enabled_(enabled) {}

  ~TestWebUIConfig() override = default;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override {
    return enabled_;
  }

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    return nullptr;
  }

 private:
  bool enabled_;
};

class TestInternalWebUIConfig : public content::InternalWebUIConfig {
 public:
  TestInternalWebUIConfig(const std::string& host, bool enabled)
      : content::InternalWebUIConfig(host), enabled_(enabled) {}

  ~TestInternalWebUIConfig() override = default;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override {
    return enabled_;
  }

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    return nullptr;
  }

 private:
  bool enabled_;
};

class MockPage : public chrome_urls::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<chrome_urls::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  mojo::Receiver<chrome_urls::mojom::Page> receiver_{this};
};

}  // namespace

class ChromeUrlsHandlerTest : public testing::Test {
 public:
  ChromeUrlsHandlerTest() : profile_(std::make_unique<TestingProfile>()) {}

  void SetUp() override {
    handler_ = std::make_unique<chrome_urls::ChromeUrlsHandler>(
        mojo::PendingReceiver<chrome_urls::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), profile_.get());
    mock_page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockPage> mock_page_;
  std::unique_ptr<chrome_urls::ChromeUrlsHandler> handler_;
};

TEST_F(ChromeUrlsHandlerTest, GetUrls) {
  // Register some test configs.
  content::ScopedWebUIConfigRegistration config(
      std::make_unique<TestWebUIConfig>(content::kChromeUIScheme, "foo", true));
  content::ScopedWebUIConfigRegistration config_internal(
      std::make_unique<TestInternalWebUIConfig>("foo-internals", true));
  content::ScopedWebUIConfigRegistration config_untrusted(
      std::make_unique<TestWebUIConfig>(content::kChromeUIUntrustedScheme,
                                        "bar", true));
  content::ScopedWebUIConfigRegistration config_disabled(
      std::make_unique<TestWebUIConfig>(content::kChromeUIScheme, "cats",
                                        false));
  content::ScopedWebUIConfigRegistration config_internal_disabled(
      std::make_unique<TestInternalWebUIConfig>("cats-internals", false));
  content::ScopedWebUIConfigRegistration config_untrusted_disabled(
      std::make_unique<TestWebUIConfig>(content::kChromeUIUntrustedScheme,
                                        "dogs", false));

#if BUILDFLAG(IS_CHROMEOS)
  // Redirect the files, and sanitize config because they assume an Ash Shell
  // exists in IsWebUIEnabled(), and the shell does not exist in unit tests.
  content::ScopedWebUIConfigRegistration replace_files_app(
      std::make_unique<TestWebUIConfig>(
          content::kChromeUIScheme, ash::file_manager::kChromeUIFileManagerHost,
          true));
  content::ScopedWebUIConfigRegistration replace_sanitize_app(
      std::make_unique<TestWebUIConfig>(content::kChromeUIScheme,
                                        ash::kChromeUISanitizeAppHost, true));
#endif  // BUILDFLAG(IS_CHROMEOS)

  base::MockCallback<chrome_urls::ChromeUrlsHandler::GetUrlsCallback> callback;
  chrome_urls::mojom::ChromeUrlsDataPtr url_data;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce([&url_data](chrome_urls::mojom::ChromeUrlsDataPtr arg) {
        url_data = std::move(arg);
      });
  handler_->GetUrls(callback.Get());

  // Validate WebUI URL data.
  bool found_foo = false;
  bool found_foo_internals = false;
  bool found_bar = false;
  bool found_cats = false;
  bool found_cats_internals = false;
  bool found_dogs = false;
  for (const auto& info : url_data->webui_urls) {
    // Check that the 6 configs added are returned, and are in the expected
    // order.
    if (info->url.spec() == "chrome://cats/") {
      found_cats = true;
      EXPECT_FALSE(info->enabled);
      EXPECT_FALSE(info->internal);
      EXPECT_FALSE(found_cats_internals || found_bar || found_dogs ||
                   found_foo || found_foo_internals);
    } else if (info->url.spec() == "chrome://cats-internals/") {
      EXPECT_TRUE(found_cats);
      found_cats_internals = true;
      EXPECT_FALSE(info->enabled);
      EXPECT_TRUE(info->internal);
      EXPECT_FALSE(found_bar || found_dogs || found_foo || found_foo_internals);
    } else if (info->url.spec() == "chrome://foo/") {
      EXPECT_TRUE(found_cats && found_cats_internals);
      found_foo = true;
      EXPECT_TRUE(info->enabled);
      EXPECT_FALSE(info->internal);
      EXPECT_FALSE(found_bar || found_dogs || found_foo_internals);
    } else if (info->url.spec() == "chrome://foo-internals/") {
      EXPECT_TRUE(found_cats && found_cats_internals && found_foo);
      found_foo_internals = true;
      EXPECT_TRUE(info->enabled);
      EXPECT_TRUE(info->internal);
      EXPECT_FALSE(found_bar || found_dogs);
    } else if (info->url.spec() == "chrome-untrusted://bar/") {
      EXPECT_TRUE(found_cats && found_cats_internals && found_foo &&
                  found_foo_internals);
      found_bar = true;
      EXPECT_TRUE(info->enabled);
      EXPECT_FALSE(found_dogs);
    } else if (info->url.spec() == "chrome-untrusted://dogs/") {
      EXPECT_TRUE(found_cats && found_cats_internals && found_foo &&
                  found_foo_internals && found_bar);
      found_dogs = true;
      EXPECT_FALSE(info->enabled);
    }
  }
  EXPECT_TRUE(found_cats && found_cats_internals && found_foo && found_bar &&
              found_dogs && found_foo_internals);

  // Validate command URL data.
  base::span<const base::cstring_view> expected_urls =
      chrome::ChromeDebugURLs();
  for (const GURL& url : url_data->command_urls) {
    EXPECT_TRUE(base::Contains(expected_urls, url.spec()));
  }
}

TEST_F(ChromeUrlsHandlerTest, SetDebugPagesEnabled) {
  // Initialize the pref to false.
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetUserPref(
      chrome_urls::kInternalOnlyUisEnabled,
      std::make_unique<base::Value>(false));
  base::MockCallback<base::RepeatingClosure> callback;
  EXPECT_CALL(callback, Run).Times(1);
  handler_->SetDebugPagesEnabled(true, callback.Get());

  // Pref value is true after SetDebugPagesEnabled() is called.
  const base::Value* pref =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->GetUserPref(
          chrome_urls::kInternalOnlyUisEnabled);
  EXPECT_TRUE(!!pref && pref->GetBool());
}

}  // namespace chrome_urls
