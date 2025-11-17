// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_handler.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/fake_tab_id_generator.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
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
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::Pointee;

class FakeActionChipsHandler : public ActionChipsHandler {
 public:
  FakeActionChipsHandler(
      mojo::PendingReceiver<action_chips::mojom::ActionChipsHandler>
          pending_receiver,
      Profile* profile,
      content::WebUI* web_ui,
      TabIdGenerator* tab_id_generator)
      : ActionChipsHandler(std::move(pending_receiver),
                           profile,
                           web_ui,
                           tab_id_generator) {}
};

struct TabInfoFields {
  int32_t tab_id = 0;
  std::string title;
  GURL url;
  base::Time last_active_time;
};

struct ActionChipFields {
  std::string title;
  std::string suggestion;
  ChipType type = ChipType::kRecentTab;
  std::optional<TabInfoFields> tab;
};

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

// Get the value used for tab_id.
SessionID::id_type GetSessionID(std::string_view title) {
  return static_cast<int32_t>(base::PersistentHash(title));
}

// Returns the value of Time at `index`-th element.
// In the current test logic, the clock proceeds by 1 second every time
// a tab is added.
base::Time GetTimeAt(const size_t index) {
  return base::Time::FromMillisecondsSinceUnixEpoch(0) +
         base::Seconds(index + 1);
}
}  // namespace

namespace action_chips::mojom {
void PrintTo(const TabInfo& tab, std::ostream* os) {
  *os << "TabInfo{\n"
      << "  tab_id: " << tab.tab_id << ",\n"
      << "  title: \"" << tab.title << "\",\n"
      << "  url: \"" << tab.url << "\",\n"
      << "  last_active_time: " << tab.last_active_time << "\n}"
      << "\n}";
}

void PrintTo(const TabInfoPtr& tab, std::ostream* os) {
  if (tab.is_null()) {
    *os << "nullptr";
  } else {
    PrintTo(*tab, os);
  }
}

void PrintTo(const ActionChip& chip, std::ostream* os) {
  *os << "ActionChip{\n"
      << "  title: \"" << chip.title << "\",\n"
      << "  suggestion: \"" << chip.suggestion << "\",\n"
      << "  type: " << chip.type << ",\n"
      << "  tab_info: ";
  if (chip.tab.is_null()) {
    *os << "nullptr";
  } else {
    PrintTo(*chip.tab, os);
  }
  *os << "\n}";
}

void PrintTo(const ActionChipPtr& chip, std::ostream* os) {
  if (chip.is_null()) {
    *os << "nullptr";
  } else {
    PrintTo(*chip, os);
  }
}
}  // namespace action_chips::mojom

class ActionChipsHandlerTest : public testing::Test {
 public:
  ActionChipsHandlerTest() = default;
  void SetUp() override {
    testing::Test::SetUp();
    CreateProfileAndWebContents();
    SetUpTabStripAndBrowserWindow();
    CreateHandler();
  }

  content::WebContents* AddTab(GURL url, const std::u16string& title) {
    auto contents = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    content::WebContentsTester* tester =
        content::WebContentsTester::For(contents.get());
    tester->NavigateAndCommit(url);
    tester->SetTitle(title);
    tester->SetLastActiveTime(base::Time::FromMillisecondsSinceUnixEpoch(0) +
                              IncrementTimeAndGet());
    content::WebContents* raw_ptr = contents.get();
    tab_strip_model_->AppendWebContents(std::move(contents), true);
    return raw_ptr;
  }

  FakeActionChipsHandler& handler() { return *handler_; }

 protected:
  content::WebContents* web_contents() { return web_contents_.get(); }

  base::HistogramTester histogram_tester_;

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
        profile_.get(), web_ui_.get(), &tab_id_generator_);
  }

  base::TimeDelta IncrementTimeAndGet() {
    time_delta_ += base::Seconds(1);
    return time_delta_;
  }

  base::TimeDelta time_delta_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<FakeActionChipsHandler> handler_;
  FakeTabIdGenerator tab_id_generator_;

  testing::NiceMock<MockBrowserWindowInterface> browser_window_interface_;
  TestTabStripModelDelegate delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  ui::UnownedUserDataHost user_data_host_;
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
            .expected_chips = {CreateStaticRecentTabChip(
                                   {.tab_id = GetSessionID("Example Tab"),
                                    .title = "Example Tab",
                                    .url = GURL("https://www.example.com"),
                                    .last_active_time = GetTimeAt(0)}),
                               CreateStaticDeepSearchChip(),
                               CreateStaticImageGenerationChip()},
        },
        StaticChipsTestCase{
            .test_name = "ThreeChipsUsingMostRecentTab",
            .tabs = {{.url = "https://www.example.com", .title = "Example Tab"},
                     {.url = "https://www.foo.com", .title = "Foo Tab"}},
            .expected_chips = {CreateStaticRecentTabChip(
                                   {.tab_id = GetSessionID("Foo Tab"),
                                    .title = "Foo Tab",
                                    .url = GURL("https://www.foo.com"),
                                    .last_active_time = GetTimeAt(1)}),
                               CreateStaticDeepSearchChip(),
                               CreateStaticImageGenerationChip()},
        },
        StaticChipsTestCase{
            .test_name = "IgnoresChromeUrls",
            .tabs = {{.url = "chrome://version", .title = "Version"},
                     {.url = "chrome://blank", .title = "Blank"}},
            .expected_chips = {CreateStaticDeepSearchChip(),
                               CreateStaticImageGenerationChip()}},
        StaticChipsTestCase{
            .test_name = "MostRecentTabIgnoringChromeUrls",
            .tabs = {{.url = "chrome://version", .title = "Version"},
                     {.url = "https://www.google.com", .title = "Google"},
                     {.url = "chrome://blank", .title = "Blank"}},
            .expected_chips = {CreateStaticRecentTabChip(
                                   {.tab_id = GetSessionID("Google"),
                                    .title = "Google",
                                    .url = GURL("https://www.google.com"),
                                    .last_active_time = GetTimeAt(1)}),
                               CreateStaticDeepSearchChip(),
                               CreateStaticImageGenerationChip()}},
    }),
    [](const testing::TestParamInfo<StaticChipsTestCase>& param_info) {
      return param_info.param.test_name;
    });

TEST_P(ActionChipsHandlerStaticChipsTest,
       GetActionChipsReturnsStaticChipsBasedOnMostRecentTab) {
  // Arrange
  for (const auto& [url, title] : GetParam().tabs) {
    AddTab(GURL(url), base::UTF8ToUTF16(title));
  }
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
  std::transform(
      expected.begin(), expected.end(), std::back_inserter(expected_buckets),
      [](const ActionChipPtr& chip) { return Bucket(chip->type, 1); });

  // Act
  base::test::TestFuture<std::vector<ActionChipPtr>> future;
  handler().GetActionChips(future.GetCallback());
  auto action_chips = future.Take();

  // Assert
  EXPECT_THAT(action_chips, ElementsAreArray(matchers));
  EXPECT_THAT(histogram_tester_.GetAllSamples("NewTabPage.ActionChips.Shown"),
              BucketsAreArray(expected_buckets));
}
