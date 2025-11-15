// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_handler.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using ::action_chips::mojom::ActionChipPtr;
using ::action_chips::mojom::ChipType;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::Pointee;

class FakeActionChipsHandler : public ActionChipsHandler {
 public:
  FakeActionChipsHandler(
      mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler>
          pending_receiver,
      Profile* profile,
      content::WebUI* web_ui)
      : ActionChipsHandler(std::move(pending_receiver), profile, web_ui) {}
};
}  // namespace

class ActionChipsHandlerTest : public testing::Test {
 public:
  ActionChipsHandlerTest() = default;
  void SetUp() override {
    testing::Test::SetUp();
    CreateProfileAndWebContents();
    SetUpTemplateURLService();
    SetUpTabStripAndBrowserWindow();
    CreateHandler();
  }

  void TearDown() override {
    tab_strip_model_->CloseAllTabs();
    template_url_service_ = nullptr;
    testing::Test::TearDown();
  }

  content::WebContents* AddTab(GURL url, const std::u16string& title) {
    auto contents = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    content::WebContentsTester* tester =
        content::WebContentsTester::For(contents.get());
    tester->NavigateAndCommit(url);
    tester->SetTitle(title);
    tester->SetLastActiveTimeTicks(IncrementTimeTicksAndGet());
    content::WebContents* raw_ptr = contents.get();
    tab_strip_model_->AppendWebContents(std::move(contents), true);
    return raw_ptr;
  }

  FakeActionChipsHandler& handler() { return *handler_; }

 protected:
  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  void CreateProfileAndWebContents() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    profile_ = profile_builder.Build();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
  }

  void SetUpTemplateURLService() {
    template_url_service_ =
        TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service_->Load();
    TemplateURLData data;
    data.SetShortName(u"Google");
    data.SetKeyword(u"google.com");
    data.SetURL("https://www.google.com/search?q={searchTerms}");
    TemplateURL* template_url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void SetUpTabStripAndBrowserWindow() {
    tab_strip_model_ =
        std::make_unique<TabStripModel>(&delegate_, profile_.get());
    ON_CALL(browser_window_interface_, GetTabStripModel())
        .WillByDefault(::testing::Return(tab_strip_model_.get()));
    ON_CALL(browser_window_interface_, GetUnownedUserDataHost)
        .WillByDefault(::testing::ReturnRef(user_data_host_));
    ON_CALL(browser_window_interface_, GetProfile())
        .WillByDefault(::testing::Return(profile_.get()));
    delegate_.SetBrowserWindowInterface(&browser_window_interface_);
    webui::SetBrowserWindowInterface(web_contents(),
                                     &browser_window_interface_);
  }

  void CreateHandler() {
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents());
    handler_ = std::make_unique<FakeActionChipsHandler>(
        mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler>(),
        profile_.get(), web_ui_.get());
  }

  base::TimeTicks IncrementTimeTicksAndGet() {
    last_active_time_ticks_ += base::Seconds(1);
    return last_active_time_ticks_;
  }

  base::TimeTicks last_active_time_ticks_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<FakeActionChipsHandler> handler_;

  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface_;
  TestTabStripModelDelegate delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  ui::UnownedUserDataHost user_data_host_;
  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

TEST_F(ActionChipsHandlerTest, GetMostRecentTab_NoTabs) {
  base::test::TestFuture<action_chips::mojom::TabInfoPtr> future;
  handler().GetMostRecentTab(future.GetCallback());
  auto tab = future.Take();

  ASSERT_TRUE(tab.is_null());
}

TEST_F(ActionChipsHandlerTest, GetMostRecentTab_SingleTab) {
  auto* expected_tab = AddTab(GURL("https://www.google.com"), u"Google");
  base::test::TestFuture<action_chips::mojom::TabInfoPtr> future;
  handler().GetMostRecentTab(future.GetCallback());
  auto tab = future.Take();

  ASSERT_FALSE(tab.is_null());
  EXPECT_EQ(tab->tab_id,
            sessions::SessionTabHelper::IdForTab(expected_tab).id());
  EXPECT_EQ(tab->url, expected_tab->GetURL());
}

TEST_F(ActionChipsHandlerTest, GetMostRecentTab_ReturnsMostRecent) {
  AddTab(GURL("https://www.google.com"), u"Google");
  auto* expected_tab = AddTab(GURL("https://www.youtube.com"), u"YouTube");

  base::test::TestFuture<action_chips::mojom::TabInfoPtr> future;
  handler().GetMostRecentTab(future.GetCallback());
  auto tab = future.Take();

  ASSERT_FALSE(tab.is_null());
  EXPECT_EQ(tab->tab_id,
            sessions::SessionTabHelper::IdForTab(expected_tab).id());
  EXPECT_EQ(tab->url, expected_tab->GetURL());
}

TEST_F(ActionChipsHandlerTest, GetMostRecentTab_IgnoresChromeUrls) {
  AddTab(GURL("chrome://version"), u"Version");
  auto* expected_tab = AddTab(GURL("https://www.google.com"), u"Google");
  AddTab(GURL("chrome://blank"), u"Blank");

  base::test::TestFuture<action_chips::mojom::TabInfoPtr> future;
  handler().GetMostRecentTab(future.GetCallback());
  auto tab = future.Take();

  ASSERT_FALSE(tab.is_null());
  EXPECT_EQ(tab->tab_id,
            sessions::SessionTabHelper::IdForTab(expected_tab).id());
  EXPECT_EQ(tab->url, expected_tab->GetURL());
}

TEST_F(ActionChipsHandlerTest, GetMostRecentTab_ReturnsNullIfAllChromeUrls) {
  AddTab(GURL("chrome://version"), u"Version");
  AddTab(GURL("chrome://blank"), u"Blank");

  base::test::TestFuture<action_chips::mojom::TabInfoPtr> future;
  handler().GetMostRecentTab(future.GetCallback());
  auto tab = future.Take();

  ASSERT_TRUE(tab.is_null());
}

TEST_F(ActionChipsHandlerTest, GetActionChipsReturnsTwoChipsWhenNoTabIsOpen) {
  base::test::TestFuture<std::vector<ActionChipPtr>> future;
  handler().GetActionChips(future.GetCallback());
  auto action_chips = future.Take();

  EXPECT_THAT(
      action_chips,
      ElementsAre(
          Pointee(FieldsAre("Research a topic", "Dive deep into something new",
                            ChipType::kDeepSearch)),
          Pointee(FieldsAre("Create image", "Add an image and reimagine it",
                            ChipType::kImage))));
}

TEST_F(ActionChipsHandlerTest,
       GetActionChipsReturnsThreeChipsWhenAnOpenTabExists) {
  AddTab(GURL("https://www.example.com"), u"Example Tab");

  base::test::TestFuture<std::vector<ActionChipPtr>> future;
  handler().GetActionChips(future.GetCallback());
  auto action_chips = future.Take();

  EXPECT_THAT(
      action_chips,
      ElementsAre(
          Pointee(FieldsAre("Example Tab", "Ask about this tab",
                            ChipType::kRecentTab)),
          Pointee(FieldsAre("Research a topic", "Dive deep into something new",
                            ChipType::kDeepSearch)),
          Pointee(FieldsAre("Create image", "Add an image and reimagine it",
                            ChipType::kImage))));
}

TEST_F(ActionChipsHandlerTest,
       GetActionChipsReturnsThreeChipsBasedOnMostRecentTab) {
  AddTab(GURL("https://www.example.com"), u"Example Tab");
  AddTab(GURL("https://www.foo.com"), u"Foo Tab");

  base::test::TestFuture<std::vector<ActionChipPtr>> future;
  handler().GetActionChips(future.GetCallback());
  auto action_chips = future.Take();

  EXPECT_THAT(
      action_chips,
      ElementsAre(
          Pointee(
              FieldsAre("Foo Tab", "Ask about this tab", ChipType::kRecentTab)),
          Pointee(FieldsAre("Research a topic", "Dive deep into something new",
                            ChipType::kDeepSearch)),
          Pointee(FieldsAre("Create image", "Add an image and reimagine it",
                            ChipType::kImage))));
}
