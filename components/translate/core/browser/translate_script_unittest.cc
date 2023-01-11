// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_script.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_ids_provider.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace translate {

class TranslateScriptTest : public testing::Test {
 public:
  TranslateScriptTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  TranslateScriptTest(const TranslateScriptTest&) = delete;
  TranslateScriptTest& operator=(const TranslateScriptTest&) = delete;

 protected:
  void SetUp() override {
    script_ = std::make_unique<TranslateScript>();
    auto* translate_download_manager = TranslateDownloadManager::GetInstance();
    translate_download_manager->set_application_locale("en");
    translate_download_manager->set_url_loader_factory(
        test_shared_loader_factory_);
  }

  void TearDown() override {
    script_.reset();
    TranslateDownloadManager::GetInstance()->ResetForTesting();
  }

  void Request() {
    script_->Request(base::BindRepeating(&TranslateScriptTest::OnComplete,
                                         base::Unretained(this)),
                     /*is_incognito=*/false);
  }

  const std::string& GetData() { return script_->data(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  network::TestURLLoaderFactory* GetTestURLLoaderFactory() {
    return &test_url_loader_factory_;
  }

 private:
  void OnComplete(bool success) {
    // No op.
  }

  // Sets up the task scheduling/task-runner environment for each test.
  base::test::TaskEnvironment task_environment_;

  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  // The translate script.
  std::unique_ptr<TranslateScript> script_;

  // Factory to create programmatic URL loaders.
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(TranslateScriptTest, CheckScriptParameters) {
  network::ResourceRequest last_resource_request;
  GetTestURLLoaderFactory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        last_resource_request = request;
      }));

  Request();

  GURL expected_url(TranslateScript::kScriptURL);
  GURL url = last_resource_request.url;
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(expected_url.DeprecatedGetOriginAsURL().spec(),
            url.DeprecatedGetOriginAsURL().spec());
  EXPECT_EQ(expected_url.path(), url.path());

  EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
            last_resource_request.credentials_mode);

  std::string expected_extra_headers =
      base::StringPrintf("%s\r\n\r\n", TranslateScript::kRequestHeader);
  net::HttpRequestHeaders extra_headers = last_resource_request.headers;
  EXPECT_EQ(expected_extra_headers, extra_headers.ToString());

  std::string always_use_ssl;
  net::GetValueForKeyInQuery(
      url, TranslateScript::kAlwaysUseSslQueryName, &always_use_ssl);
  EXPECT_EQ(std::string(TranslateScript::kAlwaysUseSslQueryValue),
            always_use_ssl);

  std::string callback;
  net::GetValueForKeyInQuery(
      url, TranslateScript::kCallbackQueryName, &callback);
  EXPECT_EQ(std::string(TranslateScript::kCallbackQueryValue), callback);

#if !BUILDFLAG(IS_IOS)
  // iOS does not have specific loaders for the isolated world.
  std::string css_loader_callback;
  net::GetValueForKeyInQuery(
      url, TranslateScript::kCssLoaderCallbackQueryName, &css_loader_callback);
  EXPECT_EQ(std::string(TranslateScript::kCssLoaderCallbackQueryValue),
            css_loader_callback);

  std::string javascript_loader_callback;
  net::GetValueForKeyInQuery(
      url,
      TranslateScript::kJavascriptLoaderCallbackQueryName,
      &javascript_loader_callback);
  EXPECT_EQ(std::string(TranslateScript::kJavascriptLoaderCallbackQueryValue),
            javascript_loader_callback);
#endif  // !BUILDFLAG(IS_IOS)
}

TEST_F(TranslateScriptTest, CheckScriptURL) {
  const std::string script_url("http://www.tamurayukari.com/mero-n.js");
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(translate::switches::kTranslateScriptURL,
                                  script_url);

  network::ResourceRequest last_resource_request;
  GetTestURLLoaderFactory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        last_resource_request = request;
      }));

  Request();

  GURL expected_url(script_url);
  GURL url = last_resource_request.url;
  EXPECT_TRUE(url.is_valid());
  EXPECT_EQ(expected_url.DeprecatedGetOriginAsURL().spec(),
            url.DeprecatedGetOriginAsURL().spec());
  EXPECT_EQ(expected_url.path(), url.path());
}

TEST_F(TranslateScriptTest, CheckResponse) {
  const std::string test_response =
      "(function() { console.log(\"Hello, world!\"); }());";
  GURL full_url = TranslateScript::GetTranslateScriptURL();
  GetTestURLLoaderFactory()->AddResponse(full_url.spec(), test_response);

  Request();
  RunUntilIdle();

  EXPECT_NE(std::string::npos, GetData().find(test_response));
}

}  // namespace translate
