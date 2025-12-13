// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_handler.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <unordered_map>
#include <vector>

#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-data-view.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_generator.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_mojo_test_utils.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/fake_tab_id_generator.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
using ::action_chips::mojom::ActionChip;
using ::action_chips::mojom::ActionChipPtr;
using ::action_chips::mojom::ChipType;
using ::action_chips::mojom::TabInfo;
using ::action_chips::mojom::TabInfoPtr;
using ::base::Bucket;
using ::base::BucketsAreArray;
using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnRef;

class MockPage : public action_chips::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<action_chips::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnActionChipsChanged,
              (std::vector<ActionChipPtr> action_chips),
              (override));

  mojo::Receiver<action_chips::mojom::Page> receiver_{this};
};

class MockActionChipsGenerator : public ActionChipsGenerator {
 public:
  MockActionChipsGenerator() = default;
  ~MockActionChipsGenerator() override = default;

  MOCK_METHOD(void,
              GenerateActionChips,
              (base::optional_ref<const tabs::TabInterface> tab,
               base::OnceCallback<void(std::vector<ActionChipPtr>)> callback),
              (override));
};

class FakeActionChipsHandler : public ActionChipsHandler {
 public:
  FakeActionChipsHandler(
      mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler>
          pending_receiver,
      mojo::PendingRemote<action_chips::mojom::Page> pending_page,
      Profile* profile,
      content::WebUI* web_ui,
      std::unique_ptr<ActionChipsGenerator> action_chips_generator)
      : ActionChipsHandler(std::move(pending_receiver),
                           std::move(pending_page),
                           profile,
                           web_ui,
                           std::move(action_chips_generator)) {}
};

struct TabInfoFields {
  int32_t tab_id = 0;
  std::string title;
  GURL url;
  base::Time last_active_time = base::Time::FromSecondsSinceUnixEpoch(0);
};

struct ActionChipFields {
  std::string title;
  std::string suggestion;
  ChipType type = ChipType::kRecentTab;
  std::optional<TabInfoFields> tab;
};

base::Time GetTimeAt(const size_t index) {
  return base::Time::FromMillisecondsSinceUnixEpoch(0) +
         base::Seconds(index + 1);
}

ActionChipPtr MakeActionChip(const ActionChipFields& fields) {
  TabInfoPtr tab;
  if (fields.tab.has_value()) {
    const TabInfoFields& tab_fields = *fields.tab;
    tab = TabInfo::New(tab_fields.tab_id, tab_fields.title, tab_fields.url,
                       tab_fields.last_active_time);
  }
  return ActionChip::New(fields.title, fields.suggestion, fields.type,
                         std::move(tab));
}

ActionChipFields CreateStaticRecentTabChip(const TabInfoFields tab) {
  return {.title = tab.title,
          .suggestion = "Ask about this tab",
          .type = ChipType::kRecentTab,
          .tab = std::move(tab)};
}

ActionChipFields CreateStaticDeepSearchChip() {
  return {.title = "Research a topic",
          .suggestion = "Dive deep into something new",
          .type = ChipType::kDeepSearch};
}

ActionChipFields CreateStaticImageGenerationChip() {
  return {.title = "Create image",
          .suggestion = "Add an image and reimagine it",
          .type = ChipType::kImage};
}

void CallWithStaticChips(
    base::optional_ref<const tabs::TabInterface> tab,
    base::OnceCallback<void(std::vector<action_chips::mojom::ActionChipPtr>)>
        callback) {
  std::vector<ActionChipPtr> chips;
  if (tab.has_value()) {
    content::WebContents& contents = *tab->GetContents();
    chips.push_back(MakeActionChip(CreateStaticRecentTabChip(
        {.title = base::UTF16ToUTF8(contents.GetTitle()),
         .url = contents.GetLastCommittedURL(),
         .last_active_time = contents.GetLastActiveTime()})));
  }
  chips.push_back(MakeActionChip(CreateStaticDeepSearchChip()));
  chips.push_back(MakeActionChip(CreateStaticImageGenerationChip()));
  std::move(callback).Run(std::move(chips));
}

class TabStripModelFixture {
 public:
  explicit TabStripModelFixture(Profile* profile) : profile_(profile) {
    tab_strip_model_ =
        std::make_unique<TabStripModel>(&delegate_, profile_.get());
    ON_CALL(browser_window_interface_, GetTabStripModel())
        .WillByDefault(Return(tab_strip_model_.get()));
    ON_CALL(browser_window_interface_, GetUnownedUserDataHost)
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(browser_window_interface_, GetProfile())
        .WillByDefault(Return(profile_.get()));
    delegate_.SetBrowserWindowInterface(&browser_window_interface_);
  }

  content::WebContents* AddTab(const GURL& url, const std::u16string& title) {
    auto contents = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    content::WebContentsTester* tester =
        content::WebContentsTester::For(contents.get());
    tester->NavigateAndCommit(url);
    tester->SetTitle(title);
    tester->SetLastActiveTime(IncrementTimeAndGet());
    content::WebContents* raw_ptr = contents.get();
    tab_strip_model_->AppendWebContents(std::move(contents), true);
    return raw_ptr;
  }
  content::WebContents* AddNtp(Profile* profile) {
    std::unique_ptr<content::WebContents> ntp =
        content::WebContentsTester::CreateTestWebContents(profile, nullptr);
    content::WebContents* ptr = ntp.get();
    tab_strip_model_->AppendWebContents(std::move(ntp),
                                        /*foreground=*/true);
    return ptr;
  }

  std::unique_ptr<content::WebContents> DiscardWebContentsAt(
      int index,
      std::unique_ptr<content::WebContents> new_contents) {
    return tab_strip_model_->DiscardWebContentsAt(index,
                                                  std::move(new_contents));
  }

  void Activate(int index) { tab_strip_model_->ActivateTabAt(index); }

  testing::NiceMock<MockBrowserWindowInterface>* browser_window_interface() {
    return &browser_window_interface_;
  }

 private:
  base::Time IncrementTimeAndGet() {
    time_ += base::Seconds(1);
    return time_;
  }

  base::Time time_ = base::Time::FromMillisecondsSinceUnixEpoch(0);
  raw_ptr<Profile> profile_;
  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface_;
  TestTabStripModelDelegate delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  ui::UnownedUserDataHost user_data_host_;
};

class ActionChipsHandlerTest : public testing::Test {
 public:
  ActionChipsHandlerTest() = default;
  void SetUp() override {
    testing::Test::SetUp();
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    profile_ = profile_builder.Build();
    tab_strip_model_fixture_ =
        std::make_unique<TabStripModelFixture>(profile_.get());
    content::WebContents* ntp =
        tab_strip_model_fixture_->AddNtp(profile_.get());

    webui::SetBrowserWindowInterface(
        ntp, tab_strip_model_fixture_->browser_window_interface());
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(ntp);

    auto mock_action_chips_generator =
        std::make_unique<MockActionChipsGenerator>();
    mock_action_chips_generator_ = mock_action_chips_generator.get();
    handler_ = std::make_unique<FakeActionChipsHandler>(
        mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler>(),
        page_.BindAndGetRemote(), profile_.get(), web_ui_.get(),
        std::move(mock_action_chips_generator));
    ON_CALL(*mock_action_chips_generator_, GenerateActionChips(_, _))
        .WillByDefault(&CallWithStaticChips);
  }

  FakeActionChipsHandler& handler() { return *handler_; }

 protected:
  content::WebContents* web_contents() { return web_ui_->GetWebContents(); }
  void AddTab(const GURL& url, const std::u16string& title) {
    tab_strip_model_fixture_->AddTab(url, title);
  }
  testing::NiceMock<MockPage> page_;

  base::HistogramTester histogram_tester_;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  // tab_strip_model_fixture_ depends on profile_.
  std::unique_ptr<TabStripModelFixture> tab_strip_model_fixture_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<FakeActionChipsHandler> handler_;
  raw_ptr<MockActionChipsGenerator> mock_action_chips_generator_ = nullptr;

  const tabs::TabModel::PreventFeatureInitializationForTesting prevent_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
};

struct UrlAndTitle {
  std::string url;
  std::string title;
};

struct StaticChipsTestCase {
  std::string test_name;
  std::vector<UrlAndTitle> tabs;
  std::vector<ActionChipFields> expected_chips;
};

// Assumption of test cases:
// - all the chips are static (= no remote suggestion is used)
class ActionChipsHandlerStaticChipsTest
    : public ActionChipsHandlerTest,
      public testing::WithParamInterface<StaticChipsTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    StaticChipsTests,
    ActionChipsHandlerStaticChipsTest,
    testing::ValuesIn({
        StaticChipsTestCase{
            .test_name = "TwoChipsWhenNoTabIsOpen",
            .tabs = {},
            .expected_chips = {CreateStaticDeepSearchChip(),
                               CreateStaticImageGenerationChip()},
        },
        StaticChipsTestCase{
            .test_name = "ThreeChipsWhenAnOpenTabExists",
            .tabs = {{.url = "https://www.example.com",
                      .title = "Example Tab"}},
            .expected_chips = {CreateStaticRecentTabChip({
                                   .title = "Example Tab",
                                   .url = GURL("https://www.example.com"),
                                   .last_active_time = GetTimeAt(0),
                               }),
                               CreateStaticDeepSearchChip(),
                               CreateStaticImageGenerationChip()},
        },
        StaticChipsTestCase{
            .test_name = "ThreeChipsUsingMostRecentTab",
            .tabs = {{.url = "https://www.example.com", .title = "Example Tab"},
                     {.url = "https://www.foo.com", .title = "Foo Tab"}},
            .expected_chips = {CreateStaticRecentTabChip({
                                   .title = "Foo Tab",
                                   .url = GURL("https://www.foo.com"),
                                   .last_active_time = GetTimeAt(1),
                               }),
                               CreateStaticDeepSearchChip(),
                               CreateStaticImageGenerationChip()},
        },
        StaticChipsTestCase{
            .test_name = "MostRecentTabIgnoringChromeUrls",
            .tabs = {{.url = "chrome://version", .title = "Version"},
                     // Note: Google homepage is not a SRP, so it's not ignored.
                     {.url = "https://www.google.com", .title = "Google"},
                     {.url = "chrome://blank", .title = "Blank"}},
            .expected_chips = {CreateStaticRecentTabChip({
                                   .title = "Google",
                                   .url = GURL("https://www.google.com"),
                                   .last_active_time = GetTimeAt(1),
                               }),
                               CreateStaticDeepSearchChip(),
                               CreateStaticImageGenerationChip()}},
        StaticChipsTestCase{
            .test_name = "IgnoresAllInvalidTabs",
            .tabs =
                {
                    {.url = "https://www.google.com/search?q=test",
                     .title = "Google SRP"},
                    {.url = "invalidUrl", .title = "Invalid URL"},
                    {.url = "about:blank", .title = "About Blank"},
                    {.url = "chrome://version",
                     .title = "Chrome Internal Page"},
                    {.url = "chrome-untrusted://terminal",
                     .title = "Untrusted Internal Page"},
                },
            .expected_chips = {CreateStaticDeepSearchChip(),
                               CreateStaticImageGenerationChip()}},
    }),
    [](const testing::TestParamInfo<StaticChipsTestCase>& param_info) {
      return param_info.param.test_name;
    });

TEST_P(ActionChipsHandlerStaticChipsTest,
       StartActionChipsRetrievalNotifiesUiWithStaticChipsBasedOnMostRecentTab) {
  // Arrange
  std::vector<ActionChipPtr> actual_chips;
  base::RunLoop run_loop;
  std::unordered_map<ChipType, int32_t> expected_chip_counts;
  const size_t expected_call_count =
      // When no tab is added,, only the StartActionChipsRetrieval calls back to
      // the UI. OTOH, one or more tabs are added, another call is made when the
      // NTP becomes active.
      GetParam().tabs.empty() ? 1 : 2;
  size_t total_call_count = 0;
  EXPECT_CALL(page_, OnActionChipsChanged(_))
      .Times(expected_call_count)
      .WillRepeatedly(
          [expected_call_count, &total_call_count, &actual_chips, &run_loop,
           &expected_chip_counts](std::vector<ActionChipPtr> action_chips) {
            for (const ActionChipPtr& chip : action_chips) {
              expected_chip_counts[chip->type]++;
            }
            total_call_count += 1;
            if (total_call_count == expected_call_count) {
              actual_chips = std::move(action_chips);
              run_loop.Quit();
            }
          });

  // Simulate the first request from the UI.
  handler().StartActionChipsRetrieval();

  // Act
  for (const auto& [url, title] : GetParam().tabs) {
    AddTab(GURL(url), base::UTF8ToUTF16(title));
  }
  // Put the tab into the foreground so the retrieval is triggered when one or
  // more tabs are added.
  tab_strip_model_fixture_->Activate(/*index=*/0);

  run_loop.Run();

  // Assert
  std::vector<ActionChipPtr> expected;
  for (const ActionChipFields& chip : GetParam().expected_chips) {
    expected.push_back(MakeActionChip(chip));
  }
  // Matcher seems to need to be copiable, so we take std::cref
  std::vector<Matcher<ActionChipPtr>> matchers;
  std::transform(expected.begin(), expected.end(), std::back_inserter(matchers),
                 [](const ActionChipPtr& chip) { return Eq(std::cref(chip)); });
  // Metrics mapping from expected chips to buckets.
  std::vector<Bucket> expected_buckets;
  std::transform(expected_chip_counts.begin(), expected_chip_counts.end(),
                 std::back_inserter(expected_buckets), [](const auto& pair) {
                   return Bucket(pair.first, pair.second);
                 });
  EXPECT_THAT(actual_chips, ElementsAreArray(matchers));
  EXPECT_THAT(histogram_tester_.GetAllSamples("NewTabPage.ActionChips.Shown"),
              BucketsAreArray(expected_buckets));
  histogram_tester_.ExpectTotalCount(
      "NewTabPage.ActionChips.Handler.ActionChipsRetrievalLatency",
      expected_call_count);
}

TEST_F(ActionChipsHandlerStaticChipsTest,
       StartActionChipsRetrievalSendsAnEmptyListWhenThereAreLessThanTwoChips) {
  std::vector<ActionChipPtr> actual_chips;
  base::RunLoop run_loop;
  EXPECT_CALL(page_, OnActionChipsChanged(_))
      .WillOnce(
          [&actual_chips, &run_loop](std::vector<ActionChipPtr> action_chips) {
            actual_chips = std::move(action_chips);
            run_loop.Quit();
          });
  EXPECT_CALL(*mock_action_chips_generator_, GenerateActionChips(_, _))
      .WillOnce(
          [](base::optional_ref<const tabs::TabInterface>,
             base::OnceCallback<void(
                 std::vector<action_chips::mojom::ActionChipPtr>)> callback) {
            std::vector<ActionChipPtr> chips;
            chips.push_back(MakeActionChip(CreateStaticDeepSearchChip()));
            std::move(callback).Run(std::move(chips));
          });
  handler().StartActionChipsRetrieval();
  run_loop.Run();
  EXPECT_THAT(actual_chips, IsEmpty());
}

TEST_F(ActionChipsHandlerTest, DiscardWebContentsDoesNotCrash) {
  // Discard the NTP. This would trigger a crash in
  // ActionChipsHandler::OnTabStripModelChanged if the kReplaced event is not
  // handled correctly, because the old_contents (which is the one handler is
  // watching) loses its UserData during discard.
  EXPECT_THAT(tab_strip_model_fixture_
                  ->DiscardWebContentsAt(
                      0, content::WebContentsTester::CreateTestWebContents(
                             profile_.get(), nullptr))
                  .get(),
              Eq(web_ui_->GetWebContents()));
}
}  // namespace
