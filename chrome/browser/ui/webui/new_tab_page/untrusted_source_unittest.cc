// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/untrusted_source.h"

#include <map>
#include <memory>
#include <string>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_loader.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeOneGoogleBarService : public OneGoogleBarService {
 public:
  explicit FakeOneGoogleBarService(signin::IdentityManager* identity_manager)
      : OneGoogleBarService(identity_manager, nullptr) {}

  void Refresh() override { NotifyObservers(); }

  void SetAdditionalQueryParams(
      const std::map<std::string, std::string>& params) override {
    additional_query_params_ = params;
  }

  std::map<std::string, std::string> additional_query_params() const {
    return additional_query_params_;
  }

 protected:
  std::map<std::string, std::string> additional_query_params_;
};

}  // namespace

class UntrustedSourceTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = MakeTestingProfile();
    untrusted_source_ = std::make_unique<UntrustedSource>(profile_.get());
    test_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    test_web_contents_getter_ =
        base::BindLambdaForTesting([&] { return test_web_contents_.get(); });
  }

  void TearDown() override {
    untrusted_source_.reset();
    test_web_contents_.reset();
    test_web_contents_getter_ = content::WebContents::Getter();
    profile_.reset();
  }

  UntrustedSourceTest() : identity_env_(nullptr) {}

  std::unique_ptr<TestingProfile> MakeTestingProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        OneGoogleBarServiceFactory::GetInstance(),
        base::BindRepeating(
            [](signin::IdentityManager* identity_manager,
               content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> {
              return std::make_unique<FakeOneGoogleBarService>(
                  identity_manager);
            },
            identity_env_.identity_manager()));
    auto profile = profile_builder.Build();
    return profile;
  }

  FakeOneGoogleBarService* one_google_bar_service() {
    return static_cast<FakeOneGoogleBarService*>(
        OneGoogleBarServiceFactory::GetForProfile(profile_.get()));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  signin::IdentityTestEnvironment identity_env_;
  std::unique_ptr<UntrustedSource> untrusted_source_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  content::WebContents::Getter test_web_contents_getter_;
};

TEST_F(UntrustedSourceTest, OneGoogleBarRequest_DefaultAsyncParam) {
  base::RunLoop run_loop;
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce([&run_loop](scoped_refptr<base::RefCountedMemory> memory) {
        run_loop.QuitClosure().Run();
      });
  untrusted_source_->StartDataRequest(
      GURL("chrome-untrusted://new-tab-page/one-google-bar"),
      test_web_contents_getter_, callback.Get());
  run_loop.Run();
  ASSERT_EQ(one_google_bar_service()->additional_query_params().count("async"),
            1u);
  ASSERT_EQ(one_google_bar_service()->additional_query_params().at("async"),
            "fixed:0");
}

TEST_F(UntrustedSourceTest, OneGoogleBarRequest_DefaultABPAsyncParam) {
  base::RunLoop run_loop;
  feature_list_.InitWithFeatures({ntp_features::kNtpOneGoogleBarAsyncBarParts},
                                 {});
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce([&run_loop](scoped_refptr<base::RefCountedMemory> memory) {
        run_loop.QuitClosure().Run();
      });

  untrusted_source_->StartDataRequest(
      GURL("chrome-untrusted://new-tab-page/one-google-bar"),
      test_web_contents_getter_, callback.Get());
  run_loop.Run();
  ASSERT_EQ(one_google_bar_service()->additional_query_params().count("async"),
            1u);
  ASSERT_EQ(one_google_bar_service()->additional_query_params().at("async"),
            "abp:1,fixed:0");
}

TEST_F(UntrustedSourceTest, OneGoogleBarRequest_ParamsEncoded) {
  base::RunLoop run_loop;
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce([&run_loop](scoped_refptr<base::RefCountedMemory> memory) {
        run_loop.QuitClosure().Run();
      });
  untrusted_source_->StartDataRequest(
      GURL(base::StrCat({"chrome-untrusted://new-tab-page/"
                         "one-google-bar?paramsencoded=",
                         base::Base64Encode("&async=fixed:0")})),
      test_web_contents_getter_, callback.Get());
  run_loop.Run();
  ASSERT_EQ(one_google_bar_service()->additional_query_params().count("async"),
            1u);
  ASSERT_EQ(one_google_bar_service()->additional_query_params().at("async"),
            "fixed:0");
}

TEST_F(UntrustedSourceTest, BackgroundRequest_Valid) {
  static constexpr uint8_t kData[] = {1, 2, 3, 4};
  ASSERT_TRUE(base::WriteFile(profile_->GetPath().AppendASCII("background.jpg"),
                              kData));

  {
    base::RunLoop run_loop;
    base::MockCallback<content::URLDataSource::GotDataCallback> callback;
    EXPECT_CALL(callback, Run(testing::_))
        .Times(1)
        .WillOnce([&](scoped_refptr<base::RefCountedMemory> memory) {
          EXPECT_EQ(base::span(kData), base::span(*memory));
          run_loop.Quit();
        });

    untrusted_source_->StartDataRequest(
        GURL("chrome-untrusted://new-tab-page/background.jpg"),
        test_web_contents_getter_, callback.Get());
    run_loop.Run();
  }

  // Verify that a valid token-prefixed URL also works.
  static constexpr char kToken[] = "1234567890ABCDEF1234567890ABCDEF";
  base::FilePath token_background_img =
      profile_->GetPath().AppendASCII(std::string(kToken) + "background.jpg");
  static constexpr uint8_t kTokenData[] = {5, 6, 7, 8};
  ASSERT_TRUE(base::WriteFile(token_background_img, kTokenData));

  {
    base::RunLoop run_loop;
    base::MockCallback<content::URLDataSource::GotDataCallback> callback;
    EXPECT_CALL(callback, Run(testing::_))
        .Times(1)
        .WillOnce([&](scoped_refptr<base::RefCountedMemory> memory) {
          EXPECT_EQ(base::span(kTokenData), base::span(*memory));
          run_loop.Quit();
        });

    untrusted_source_->StartDataRequest(
        GURL(std::string("chrome-untrusted://new-tab-page/") + kToken +
             "background.jpg"),
        test_web_contents_getter_, callback.Get());
    run_loop.Run();
  }
}

// Verifies that directory traversal is disallowed (crbug.com/497241148).
TEST_F(UntrustedSourceTest, BackgroundRequest_DirectoryTraversal) {
  base::FilePath extensions_dir =
      profile_->GetPath().AppendASCII("12345678901234567890123456789012");
  ASSERT_TRUE(base::CreateDirectory(extensions_dir));
  base::FilePath background_img = extensions_dir.AppendASCII("background.jpg");
  static constexpr uint8_t kData[] = {1, 2, 3, 4};
  ASSERT_TRUE(base::WriteFile(background_img, kData));

  base::RunLoop run_loop;
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce([&](scoped_refptr<base::RefCountedMemory> memory) {
        EXPECT_TRUE(base::span(*memory).empty());
        run_loop.Quit();
      });

  untrusted_source_->StartDataRequest(
      GURL("chrome-untrusted://new-tab-page/12345678901234567890123456789012/"
           "background.jpg"),
      test_web_contents_getter_, callback.Get());
  run_loop.Run();
}
