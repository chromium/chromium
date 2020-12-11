// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/ui/search/ntp_user_data_logger.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/common/search/omnibox.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class MockInstantService : public InstantService {
 public:
  explicit MockInstantService(Profile* profile) : InstantService(profile) {}
  ~MockInstantService() override = default;

  MOCK_METHOD1(AddObserver, void(InstantServiceObserver*));
  MOCK_METHOD0(UpdateNtpTheme, void());
};

class MockNTPUserDataLogger : public NTPUserDataLogger {
 public:
  MockNTPUserDataLogger() : NTPUserDataLogger(nullptr) {}
  ~MockNTPUserDataLogger() override = default;
};

class MockPage : public new_tab_page::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<new_tab_page::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD1(SetMostVisitedInfo,
               void(new_tab_page::mojom::MostVisitedInfoPtr));
  MOCK_METHOD1(SetTheme, void(new_tab_page::mojom::ThemePtr));
  MOCK_METHOD1(SetFakeboxFocused, void(bool));
  MOCK_METHOD1(SetFakeboxVisible, void(bool));
  MOCK_METHOD1(SetModulesVisible, void(bool));
  MOCK_METHOD1(AutocompleteResultChanged,
               void(search::mojom::AutocompleteResultPtr));
  MOCK_METHOD3(AutocompleteMatchImageAvailable,
               void(uint32_t, const GURL&, const std::string&));

  mojo::Receiver<new_tab_page::mojom::Page> receiver_{this};
};

}  // namespace

class NewTabPageHandlerTest : public testing::Test {
 public:
  NewTabPageHandlerTest()
      : mock_instant_service_(&profile_),
        web_contents_(factory_.CreateWebContents(&profile_)) {}

  ~NewTabPageHandlerTest() override = default;

  void SetUp() override {
    EXPECT_CALL(mock_instant_service_, AddObserver)
        .WillOnce(DoAll(testing::SaveArg<0>(&instant_service_observer_)));
    EXPECT_CALL(mock_instant_service_, UpdateNtpTheme());
    handler_ = std::make_unique<NewTabPageHandler>(
        mojo::PendingReceiver<new_tab_page::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), &profile_, &mock_instant_service_,
        web_contents_, &logger_, base::Time::Now());
    EXPECT_EQ(handler_.get(), instant_service_observer_);
  }

  void TearDown() override { testing::Test::TearDown(); }

 protected:
  testing::NiceMock<MockPage> mock_page_;
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  MockInstantService mock_instant_service_;
  content::TestWebContentsFactory factory_;
  content::WebContents* web_contents_;  // Weak. Owned by factory_.
  MockNTPUserDataLogger logger_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<NewTabPageHandler> handler_;
  InstantServiceObserver* instant_service_observer_;
};

TEST_F(NewTabPageHandlerTest, SetMostVisitedInfo) {
  EXPECT_CALL(mock_page_, SetMostVisitedInfo(testing::_));
  InstantMostVisitedInfo info;
  instant_service_observer_->MostVisitedInfoChanged(info);
}

TEST_F(NewTabPageHandlerTest, SetTheme) {
  EXPECT_CALL(mock_page_, SetTheme(testing::_));
  NtpTheme theme;
  instant_service_observer_->NtpThemeChanged(theme);
}

TEST_F(NewTabPageHandlerTest, Histograms) {
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleDismissedHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleRestoredHistogram, 0);

  handler_->OnDismissModule("shopping_tasks");
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleDismissedHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(NewTabPageHandler::kModuleDismissedHistogram) +
          ".shopping_tasks",
      1);

  handler_->OnRestoreModule("kaleidoscope");
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleRestoredHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(NewTabPageHandler::kModuleRestoredHistogram) +
          ".kaleidoscope",
      1);
}
