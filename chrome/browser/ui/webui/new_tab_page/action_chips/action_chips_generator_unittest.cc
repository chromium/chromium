// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_generator.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind_internal.h"
#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_metrics.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_mojo_test_utils.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/fake_tab_id_generator.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/remote_suggestions_service_simple.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/search/ntp_features.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "third_party/omnibox_proto/tool_mode.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
using ::action_chips::ActionChipsRequestStatus;
using ::action_chips::RemoteSuggestionsServiceSimple;
using ::action_chips::mojom::ActionChip;
using ::action_chips::mojom::ActionChipPtr;
using ::action_chips::mojom::CreateFormattedString;
using ::action_chips::mojom::FormattedString;
using ::action_chips::mojom::FormattedStringPtr;
using ::action_chips::mojom::IconType;
using ::action_chips::mojom::SuggestTemplateInfo;
using ::action_chips::mojom::TabInfo;
using ::action_chips::mojom::TabInfoPtr;
using ::action_chips::mojom::ToolMode;
using ::sync_preferences::TestingPrefServiceSyncable;
using ::tabs::TabInterface;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::TypedEq;
using ::testing::WithArg;

using enum omnibox::ToolMode;

struct CreateSuggestionOptions {
  std::optional<omnibox::GroupId> group_id;
  int32_t icon_type = omnibox::SuggestTemplateInfo::ICON_TYPE_UNSPECIFIED;
  std::vector<int> subtypes;
  std::string match_contents;
  std::string annotation;
  std::string primary_a11y_text;
  std::string secondary_a11y_text;
  std::u16string suggestion;
  omnibox::SuggestType suggest_type = omnibox::SuggestType::TYPE_FUSEBOX_ACTION;
  omnibox::ToolMode preselected_tool = omnibox::ToolMode::TOOL_MODE_UNSPECIFIED;
};

SearchSuggestionParser::SuggestResult CreateSuggestion(
    const CreateSuggestionOptions& options) {
  SearchSuggestionParser::SuggestResult result(
      options.suggestion, AutocompleteMatchType::SEARCH_SUGGEST,
      options.suggest_type, options.subtypes,
      base::UTF8ToUTF16(options.match_contents),
      /*match_contents_prefix=*/u"", base::UTF8ToUTF16(options.annotation),
      omnibox::EntityInfo::default_instance(), /*deletion_url=*/"",
      /*from_keyword=*/false, omnibox::NavigationalIntent::NAV_INTENT_NONE,
      /*relevance=*/100, /*relevance_from_server=*/true,
      /*should_prefetch=*/false,
      /*should_prerender=*/false, /*input_text=*/u"");
  if (options.group_id.has_value()) {
    result.set_suggestion_group_id(options.group_id.value());

    omnibox::SuggestTemplateInfo suggest_template_info;
    suggest_template_info.set_type_icon(
        static_cast<omnibox::SuggestTemplateInfo::IconType>(options.icon_type));
    *suggest_template_info.mutable_primary_text()->mutable_text() =
        options.match_contents;
    if (!options.primary_a11y_text.empty()) {
      *suggest_template_info.mutable_primary_text()->mutable_a11y_text() =
          options.primary_a11y_text;
    }
    *suggest_template_info.mutable_secondary_text()->mutable_text() =
        options.annotation;
    if (!options.secondary_a11y_text.empty()) {
      *suggest_template_info.mutable_secondary_text()->mutable_a11y_text() =
          options.secondary_a11y_text;
    }
    if (options.preselected_tool) {
      suggest_template_info.mutable_fusebox_action()->set_preselected_tool(
          options.preselected_tool);
    }
    result.SetSuggestTemplateInfo(std::move(suggest_template_info));
  }
  return result;
}

ActionChipPtr CreateActionChip(
    const std::string& suggestion,
    action_chips::mojom::SuggestTemplateInfoPtr suggest_template_info,
    TabInfoPtr tab) {
  auto chip = ActionChip::New();
  chip->suggestion = suggestion;
  chip->suggest_template_info = std::move(suggest_template_info);
  chip->tab = std::move(tab);
  return chip;
}

class MockRemoteSuggestionsServiceSimple
    : public RemoteSuggestionsServiceSimple {
 public:
  MockRemoteSuggestionsServiceSimple() = default;
  ~MockRemoteSuggestionsServiceSimple() override = default;

  MOCK_METHOD(std::unique_ptr<network::SimpleURLLoader>,
              GetDeepdiveChipSuggestionsForTab,
              (const std::u16string_view title,
               const GURL& url,
               base::OnceCallback<void(ActionChipSuggestionsResult&&)>),
              (override));
  MOCK_METHOD(
      std::unique_ptr<network::SimpleURLLoader>,
      GetActionChipSuggestions,
      (base::optional_ref<const std::u16string> title,
       base::optional_ref<const GURL> url,
       base::span<const omnibox::ToolMode> allowed_tools,
       base::optional_ref<const omnibox::PageVertical> page_vertical,
       base::OnceCallback<
           void(RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
           callback),
      (override));
};

int32_t GetTabHandleId(const tabs::TabInterface* tab) {
  return FakeTabIdGenerator::Get()->GenerateTabHandleId(tab);
}

TabInfoPtr CreateTabInfo(const tabs::TabInterface* tab) {
  if (tab == nullptr) {
    return nullptr;
  }
  return TabInfo::New(GetTabHandleId(tab),
                      base::UTF16ToUTF8(tab->GetContents()->GetTitle()),
                      tab->GetContents()->GetLastCommittedURL(),
                      base::Time::FromMillisecondsSinceUnixEpoch(0));
}

ActionChipPtr CreateStaticRecentTabChip(TabInfoPtr tab) {
  const std::string title = "Ask about previous tab";
  // Clone tab title before move.
  const std::string subtitle = tab->title;
  return CreateActionChip(
      "",
      SuggestTemplateInfo::New(IconType::kFavicon, CreateFormattedString(title),
                               CreateFormattedString(subtitle),
                               ToolMode::kUnspecified),
      std::move(tab));
}

const ActionChipPtr& GetStaticDeepSearchChip() {
  static const base::NoDestructor<ActionChipPtr> kInstance(CreateActionChip(
      /*suggestion=*/"",
      SuggestTemplateInfo::New(
          IconType::kGlobeWithSearchLoop, CreateFormattedString("Deep Search"),
          CreateFormattedString("Dive deep into something new"),
          ToolMode::kDeepSearch),
      /*tab=*/nullptr));
  return *kInstance;
}

const ActionChipPtr& GetStaticImageGenerationChip() {
  static const base::NoDestructor<ActionChipPtr> kInstance(CreateActionChip(
      /*suggestion=*/"",
      SuggestTemplateInfo::New(
          IconType::kBanana, CreateFormattedString("Create images"),
          CreateFormattedString("Add an image and reimagine it"),
          ToolMode::kImageGen),
      /*tab=*/nullptr));
  return *kInstance;
}

const ActionChipPtr& GetStaticCanvasChip() {
  static const base::NoDestructor<ActionChipPtr> kInstance(CreateActionChip(
      /*suggestion=*/"",
      SuggestTemplateInfo::New(IconType::kDraftSpark,
                               CreateFormattedString(l10n_util::GetStringUTF8(
                                   IDS_NTP_ACTION_CHIP_CANVAS_HEADING)),
                               CreateFormattedString(l10n_util::GetStringUTF8(
                                   IDS_NTP_ACTION_CHIP_CANVAS_BODY)),
                               ToolMode::kCanvas),
      /*tab=*/nullptr));
  return *kInstance;
}

// A container to store WebContents and its dependency.
// The main usage is to populate TabInterface.
class TabFixture {
 public:
  TabFixture(const GURL& url, const std::u16string& title) {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    profile_ = profile_builder.Build();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    content::WebContentsTester* tester =
        content::WebContentsTester::For(web_contents_.get());
    tester->NavigateAndCommit(url);
    tester->SetTitle(title);
    tester->SetLastActiveTime(base::Time::FromMillisecondsSinceUnixEpoch(0));

    ON_CALL(mock_tab_, GetContents())
        .WillByDefault(Return(web_contents_.get()));
  }

  tabs::MockTabInterface& mock_tab() { return mock_tab_; }

 private:
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  tabs::MockTabInterface mock_tab_;
};

// The fixture to initialize the environment. The fields in this class
// usually do not have to be touched in each test case.
class EnvironmentFixture {
  // private since there is no reason to make them accessible.
  content::BrowserTaskEnvironment env;
  content::RenderViewHostTestEnabler rvh_test_enabler;
};

// A fixture to set up the unit under test.
class GeneratorFixture {
 public:
  GeneratorFixture() {
    auto client = std::make_unique<FakeAutocompleteProviderClient>();
    fake_client_ = client.get();
    auto service = std::make_unique<MockRemoteSuggestionsServiceSimple>();
    mock_service_ = service.get();

    // Preferences are read at object initialization and thus must be set up
    // before the eligibility service is instantiated.
    AimEligibilityService::RegisterProfilePrefs(pref_service_.registry());

    mock_aim_eligibility_service_ = std::make_unique<MockAimEligibilityService>(
        pref_service_, nullptr, nullptr, nullptr,
        AimEligibilityService::Configuration{});

    generator_ = std::make_unique<ActionChipsGeneratorImpl>(
        FakeTabIdGenerator::Get(), mock_aim_eligibility_service_.get(),
        std::move(client), std::move(service));

    ON_CALL(*fake_client_, IsPersonalizedUrlDataCollectionActive())
        .WillByDefault(Return(true));

    set_searchbox_config({TOOL_MODE_DEEP_SEARCH, TOOL_MODE_IMAGE_GEN});
  }

  // This method is created to make it easy to pass `const
  // MockTabInterface*`.
  void GenerateActionChips(const tabs::TabInterface* tab,
                           base::RunLoop& run_loop,
                           std::vector<ActionChipPtr>& actual) {
    GenerateActionChips(base::optional_ref(tab), run_loop, actual);
  }

  void GenerateActionChips(base::optional_ref<const tabs::TabInterface> tab,
                           base::RunLoop& run_loop,
                           std::vector<ActionChipPtr>& actual) {
    generator_->GenerateActionChips(
        tab, base::BindLambdaForTesting(
                 [&run_loop, &actual](std::vector<ActionChipPtr> chips) {
                   actual = std::move(chips);
                   run_loop.Quit();
                 }));
  }

  FakeAutocompleteProviderClient& fake_client() { return *fake_client_; }

  MockRemoteSuggestionsServiceSimple& mock_service() { return *mock_service_; }

  MockAimEligibilityService& mock_aim_eligibility_service() {
    return *mock_aim_eligibility_service_;
  }

  void set_searchbox_config(base::span<const omnibox::ToolMode> allowed_tools) {
    mock_aim_eligibility_service_->config().clear_tool_configs();
    for (const auto tool : allowed_tools) {
      mock_aim_eligibility_service_->config().add_tool_configs()->set_tool(
          tool);
    }
  }

 private:
  TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<MockAimEligibilityService> mock_aim_eligibility_service_;
  std::unique_ptr<ActionChipsGeneratorImpl> generator_;
  raw_ptr<FakeAutocompleteProviderClient> fake_client_ = nullptr;
  raw_ptr<MockRemoteSuggestionsServiceSimple> mock_service_ = nullptr;
};

TEST(ActionChipGeneratorTest, GenerateThreeStaticChipsWhenNoTabIsPassed) {
  EnvironmentFixture env;
  GeneratorFixture generator_fixture;
  base::RunLoop run_loop;
  base::test::ScopedFeatureList list;
  list.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpNextFeatures,
        {{ntp_features::kNtpNextShowStaticTextParam.name, "true"}}},
       {ntp_features::kNtpNextCanvasChip, {}}},
      {});

  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(std::nullopt, run_loop, actual);
  run_loop.Run();
  EXPECT_THAT(actual, ElementsAre(Eq(std::cref(GetStaticImageGenerationChip())),
                                  Eq(std::cref(GetStaticCanvasChip())),
                                  Eq(std::cref(GetStaticDeepSearchChip()))));
}

TEST(ActionChipGeneratorTest,
     GenerateStaticChipsWhenNtpNextShowStaticTextParamIsTrue) {
  EnvironmentFixture env;
  const GURL page_url("https://google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
}

TEST(ActionChipGeneratorTest,
     GenerateStaticChipsWithoutCanvasChipWhenCanvasFlagDisabled) {
  EnvironmentFixture env;
  const GURL page_url("https://google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  EXPECT_CALL(generator_fixture.mock_aim_eligibility_service(),
              IsDeepSearchEligible())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(generator_fixture.mock_aim_eligibility_service(),
              IsCreateImagesEligible())
      .WillRepeatedly(Return(true));
  // IsCanvasEligible should NOT be called because the flag is disabled.
  EXPECT_CALL(generator_fixture.mock_aim_eligibility_service(),
              IsCanvasEligible())
      .Times(0);

  base::test::ScopedFeatureList list;
  list.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpNextFeatures,
        {{ntp_features::kNtpNextShowStaticTextParam.name, "true"},
         {ntp_features::kNtpNextShowStaticRecentTabChipParam.name, "true"}}}},
      {ntp_features::kNtpNextCanvasChip});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  ActionChipPtr most_recent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));

  // Order: RecentTab, DeepSearch, CreateImage. Canvas absent.
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_recent_tab_chip)),
                          Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
}

struct StaticChipsGenerationWithAimEligibilityTestCase {
  // Whether the most recent tab exists.
  bool tab_exists = false;
  // Whether the user is eligible for deep search.
  bool is_deepsearch_eligible = false;
  // Whether the user is eligible for image creation.
  bool is_create_images_eligible = false;
  // Whether the user is eligible for canvas.
  bool is_canvas_eligible = false;
  using TupleT = std::tuple<bool, bool, bool, bool>;
  explicit StaticChipsGenerationWithAimEligibilityTestCase(TupleT tuple)
      : tab_exists(get<0>(tuple)),
        is_deepsearch_eligible(get<1>(tuple)),
        is_create_images_eligible(get<2>(tuple)),
        is_canvas_eligible(get<3>(tuple)) {}
};

using ActionChipGeneratorStaticChipsGenerationWithAimEligibilityTest =
    testing::TestWithParam<StaticChipsGenerationWithAimEligibilityTestCase>;
INSTANTIATE_TEST_SUITE_P(
    ActionChipGeneratorTests,
    ActionChipGeneratorStaticChipsGenerationWithAimEligibilityTest,
    ::testing::ConvertGenerator<
        StaticChipsGenerationWithAimEligibilityTestCase::TupleT>(
        testing::Combine(testing::Bool(),
                         testing::Bool(),
                         testing::Bool(),
                         testing::Bool())),
    [](const testing::TestParamInfo<
        StaticChipsGenerationWithAimEligibilityTestCase>& param_info) {
      const StaticChipsGenerationWithAimEligibilityTestCase& param =
          param_info.param;
      std::string test_name;
      if (!param.is_deepsearch_eligible && !param.is_create_images_eligible &&
          !param.is_canvas_eligible) {
        test_name = "NoEligibility";
      } else {
        test_name = "EligibilityFor";
        if (param.is_deepsearch_eligible) {
          test_name += "DeepSearch";
        }
        if (param.is_create_images_eligible) {
          test_name += "ImageCreation";
        }
        if (param.is_canvas_eligible) {
          test_name += "Canvas";
        }
      }

      if (param.tab_exists) {
        test_name += "AndTabExists";
      }
      return test_name;
    });

TEST_P(ActionChipGeneratorStaticChipsGenerationWithAimEligibilityTest,
       GenerateStaticChipsWhenNtpNextShowStaticTextParamIsTrue) {
  EnvironmentFixture env;
  std::optional<TabFixture> tab_fixture;
  if (GetParam().tab_exists) {
    tab_fixture.emplace(GURL("https://google.com/"), u"Google");
  }
  GeneratorFixture generator_fixture;

  EXPECT_CALL(generator_fixture.mock_aim_eligibility_service(),
              IsDeepSearchEligible())
      .Times(1)
      .WillRepeatedly(Return(GetParam().is_deepsearch_eligible));
  EXPECT_CALL(generator_fixture.mock_aim_eligibility_service(),
              IsCreateImagesEligible())
      .WillRepeatedly(Return(GetParam().is_create_images_eligible));
  EXPECT_CALL(generator_fixture.mock_aim_eligibility_service(),
              IsCanvasEligible())
      .WillRepeatedly(Return(GetParam().is_canvas_eligible));

  base::test::ScopedFeatureList list;
  list.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpNextFeatures,
        {{ntp_features::kNtpNextShowStaticTextParam.name, "true"}}},
       {ntp_features::kNtpNextCanvasChip, {}}},
      {});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  TabInterface* tab =
      tab_fixture.has_value() ? &tab_fixture->mock_tab() : nullptr;
  generator_fixture.GenerateActionChips(tab, run_loop, actual);
  run_loop.Run();

  std::vector<Matcher<ActionChipPtr>> expected;
  if (GetParam().is_create_images_eligible) {
    expected.push_back(Eq(std::cref(GetStaticImageGenerationChip())));
  }
  if (GetParam().is_canvas_eligible) {
    expected.push_back(Eq(std::cref(GetStaticCanvasChip())));
  }
  if (GetParam().is_deepsearch_eligible) {
    expected.push_back(Eq(std::cref(GetStaticDeepSearchChip())));
  }
  EXPECT_THAT(actual, ElementsAreArray(expected));
}

TEST(ActionChipGeneratorTest,
     CallsSuggestionsEndpointWithoutTabInformationWhenHistorySyncIsOptOut) {
  EnvironmentFixture env;
  const GURL page_url("https://google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;
  EXPECT_CALL(generator_fixture.fake_client(),
              IsPersonalizedUrlDataCollectionActive())
      .WillOnce(Return(false));

  EXPECT_CALL(
      generator_fixture.mock_service(),
      GetActionChipSuggestions(Eq(std::nullopt), Eq(std::nullopt), _, _, _))
      .Times(1)
      .WillOnce(WithArg<4>(
          [](base::OnceCallback<void(
                 RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
                 callback) {
            std::move(callback).Run(
                base::unexpected(RemoteSuggestionsServiceSimple::Error{
                    RemoteSuggestionsServiceSimple::ParseError{
                        .parse_failure_reason = RemoteSuggestionsServiceSimple::
                            ParseFailureReason::kResponseEmpty}}));
            return nullptr;
          }));

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
}

TEST(ActionChipGeneratorTest, SteadyStateWithNewEndpoint) {
  EnvironmentFixture env;
  base::HistogramTester histogram_tester;
  const GURL page_url("https://www.google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  const std::string recent_tab_title = "Ask about previous tab";
  const std::string recent_tab_subtitle = "Subtitle for steady recent tab";
  const std::u16string recent_tab_suggestion =
      u"Suggestion for steady recent tab";
  const std::string deep_search_title = "Research a topic";
  const std::string deep_search_subtitle = "Subtitle for steady deep search";
  const std::u16string deep_search_suggestion =
      u"Suggestion for steady deep search";
  const std::string image_gen_title = "Create image";
  const std::string image_gen_subtitle = "Subtitle for steady image gen";
  const std::u16string image_gen_suggestion =
      u"Suggestion for steady image gen";

  EXPECT_CALL(
      generator_fixture.mock_service(),
      GetActionChipSuggestions(Eq(page_title), Eq(page_url),
                               ElementsAre(omnibox::TOOL_MODE_DEEP_SEARCH,
                                           omnibox::TOOL_MODE_IMAGE_GEN),
                               Eq(std::nullopt), _))
      .WillOnce(WithArg<4>([&](base::OnceCallback<void(
                                   RemoteSuggestionsServiceSimple::
                                       ActionChipSuggestionsResult&&)>
                                   callback) {
        std::move(callback).Run(SearchSuggestionParser::SuggestResults{
            CreateSuggestion(
                {.group_id = omnibox::GROUP_AI_MODE_CONTEXTUAL_SEARCH_ACTION,
                 .icon_type = omnibox::SuggestTemplateInfo::FAVICON,
                 .match_contents = recent_tab_title,
                 .annotation = recent_tab_subtitle,
                 .suggestion = recent_tab_suggestion}),
            CreateSuggestion(
                {.group_id = omnibox::GROUP_AI_MODE_DEEP_SEARCH_ACTION,
                 .icon_type =
                     omnibox::SuggestTemplateInfo::GLOBE_WITH_SEARCH_LOOP,
                 .match_contents = deep_search_title,
                 .annotation = deep_search_subtitle,
                 .suggestion = deep_search_suggestion,
                 .preselected_tool = omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH}),
            CreateSuggestion(
                {.group_id = omnibox::GROUP_AI_MODE_CREATE_IMAGE_ACTION,
                 .icon_type = omnibox::SuggestTemplateInfo::BANANA,
                 .match_contents = image_gen_title,
                 .annotation = image_gen_subtitle,
                 .suggestion = image_gen_suggestion,
                 .preselected_tool = omnibox::ToolMode::TOOL_MODE_IMAGE_GEN})});
        return nullptr;
      }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  TabInfoPtr tab_info = CreateTabInfo(&tab_fixture.mock_tab());
  ActionChipPtr chip0 = CreateActionChip(
      base::UTF16ToUTF8(recent_tab_suggestion),
      SuggestTemplateInfo::New(
          IconType::kFavicon, CreateFormattedString(recent_tab_title),
          CreateFormattedString(recent_tab_subtitle), ToolMode::kUnspecified),
      tab_info->Clone());
  ActionChipPtr chip1 = CreateActionChip(
      base::UTF16ToUTF8(deep_search_suggestion),
      SuggestTemplateInfo::New(IconType::kGlobeWithSearchLoop,
                               CreateFormattedString(deep_search_title),
                               CreateFormattedString(deep_search_subtitle),
                               ToolMode::kDeepSearch),
      nullptr);
  ActionChipPtr chip2 = CreateActionChip(
      base::UTF16ToUTF8(image_gen_suggestion),
      SuggestTemplateInfo::New(
          IconType::kBanana, CreateFormattedString(image_gen_title),
          CreateFormattedString(image_gen_subtitle), ToolMode::kImageGen),
      nullptr);

  EXPECT_THAT(actual, ElementsAre(Eq(std::cref(chip0)), Eq(std::cref(chip1)),
                                  Eq(std::cref(chip2))));
  histogram_tester.ExpectUniqueSample("NewTabPage.ActionChips.RequestStatus",
                                      ActionChipsRequestStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample("NewTabPage.ActionChips.SuggestionCount",
                                      3, 1);
}

TEST(ActionChipGeneratorTest, SteadyStateWithNewEndpointAndNoTab) {
  EnvironmentFixture env;
  GeneratorFixture generator_fixture;

  const std::string deep_search_title = "Research a topic";
  const std::string deep_search_subtitle = "Subtitle for deep search";
  const std::u16string deep_search_suggestion = u"Interior design courses";
  const std::string image_gen_title = "Create image";
  const std::string image_gen_subtitle = "Subtitle for image gen";
  const std::u16string image_gen_suggestion = u"Show me a city skyline";

  EXPECT_CALL(
      generator_fixture.mock_service(),
      GetActionChipSuggestions(Eq(std::nullopt), Eq(std::nullopt),
                               ElementsAre(omnibox::TOOL_MODE_DEEP_SEARCH,
                                           omnibox::TOOL_MODE_IMAGE_GEN),
                               Eq(std::nullopt), _))
      .WillOnce(WithArg<4>(
          [&](base::OnceCallback<void(RemoteSuggestionsServiceSimple::
                                          ActionChipSuggestionsResult&&)>
                  callback) {
            std::move(callback).Run(SearchSuggestionParser::SuggestResults{
                CreateSuggestion(
                    {.group_id = omnibox::GROUP_AI_MODE_DEEP_SEARCH_ACTION,
                     .icon_type =
                         omnibox::SuggestTemplateInfo::GLOBE_WITH_SEARCH_LOOP,
                     .match_contents = deep_search_title,
                     .annotation = deep_search_subtitle,
                     .suggestion = deep_search_suggestion,
                     .preselected_tool =
                         omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH}),
                CreateSuggestion(
                    {.group_id = omnibox::GROUP_AI_MODE_CREATE_IMAGE_ACTION,
                     .icon_type = omnibox::SuggestTemplateInfo::BANANA,
                     .match_contents = image_gen_title,
                     .annotation = image_gen_subtitle,
                     .suggestion = image_gen_suggestion,
                     .preselected_tool =
                         omnibox::ToolMode::TOOL_MODE_IMAGE_GEN})});
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(std::nullopt, run_loop, actual);
  run_loop.Run();

  ActionChipPtr chip0 = CreateActionChip(
      base::UTF16ToUTF8(deep_search_suggestion),
      SuggestTemplateInfo::New(IconType::kGlobeWithSearchLoop,
                               CreateFormattedString(deep_search_title),
                               CreateFormattedString(deep_search_subtitle),
                               ToolMode::kDeepSearch),
      nullptr);
  ActionChipPtr chip1 = CreateActionChip(
      base::UTF16ToUTF8(image_gen_suggestion),
      SuggestTemplateInfo::New(
          IconType::kBanana, CreateFormattedString(image_gen_title),
          CreateFormattedString(image_gen_subtitle), ToolMode::kImageGen),
      nullptr);

  std::vector<Matcher<const ActionChipPtr&>> expected;
  expected.push_back(Eq(std::cref(chip0)));
  expected.push_back(Eq(std::cref(chip1)));

  EXPECT_THAT(actual, ElementsAreArray(expected));
}

TEST(ActionChipGeneratorTest, NewEndpointFailureFallsBackToStaticChips) {
  EnvironmentFixture env;
  base::HistogramTester histogram_tester;
  const GURL page_url("https://www.google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  EXPECT_CALL(generator_fixture.mock_service(),
              GetActionChipSuggestions(Eq(page_title), Eq(page_url), _, _, _))
      .WillOnce(WithArg<4>(
          [](base::OnceCallback<void(
                 RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
                 callback) {
            std::move(callback).Run(
                base::unexpected(RemoteSuggestionsServiceSimple::NetworkError{
                    .net_error = net::ERR_TIMED_OUT}));
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));

  histogram_tester.ExpectUniqueSample("NewTabPage.ActionChips.RequestStatus",
                                      ActionChipsRequestStatus::kNetworkError,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.ActionChips.RequestStatus.NetworkError",
      std::abs(net::ERR_TIMED_OUT), 1);
}

TEST(ActionChipGeneratorTest, NewEndpointOptOutReturnsEndpointChips) {
  EnvironmentFixture env;
  const GURL page_url("https://www.google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  const std::string deep_search_title = "Research a topic";
  const std::string deep_search_subtitle = "Subtitle for deep search";
  const std::string deep_search_suggestion = "Interior design courses";
  const std::string image_gen_title = "Create image";
  const std::string image_gen_subtitle = "Subtitle for image gen";
  const std::string image_gen_suggestion = "Show me a city skyline";

  EXPECT_CALL(generator_fixture.fake_client(),
              IsPersonalizedUrlDataCollectionActive())
      .WillOnce(Return(false));

  // Should call remote service with nullopt title and URL.
  EXPECT_CALL(generator_fixture.mock_service(),
              GetActionChipSuggestions(Eq(std::nullopt), Eq(std::nullopt), _,
                                       Eq(std::nullopt), _))
      .WillOnce(WithArg<4>(
          [&](base::OnceCallback<void(RemoteSuggestionsServiceSimple::
                                          ActionChipSuggestionsResult&&)>
                  callback) {
            std::move(callback).Run(SearchSuggestionParser::SuggestResults{
                CreateSuggestion(
                    {.group_id = omnibox::GROUP_AI_MODE_DEEP_SEARCH_ACTION,
                     .icon_type =
                         omnibox::SuggestTemplateInfo::GLOBE_WITH_SEARCH_LOOP,
                     .match_contents = deep_search_title,
                     .annotation = deep_search_subtitle,
                     .suggestion = base::UTF8ToUTF16(deep_search_suggestion),
                     .preselected_tool =
                         omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH}),
                CreateSuggestion(
                    {.group_id = omnibox::GROUP_AI_MODE_CREATE_IMAGE_ACTION,
                     .icon_type = omnibox::SuggestTemplateInfo::BANANA,
                     .match_contents = image_gen_title,
                     .annotation = image_gen_subtitle,
                     .suggestion = base::UTF8ToUTF16(image_gen_suggestion),
                     .preselected_tool =
                         omnibox::ToolMode::TOOL_MODE_IMAGE_GEN})});
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  // Expect endpoint chips.
  ActionChipPtr chip0 = CreateActionChip(
      deep_search_suggestion,
      SuggestTemplateInfo::New(IconType::kGlobeWithSearchLoop,
                               CreateFormattedString(deep_search_title),
                               CreateFormattedString(deep_search_subtitle),
                               ToolMode::kDeepSearch),
      nullptr);
  ActionChipPtr chip1 = CreateActionChip(
      image_gen_suggestion,
      SuggestTemplateInfo::New(
          IconType::kBanana, CreateFormattedString(image_gen_title),
          CreateFormattedString(image_gen_subtitle), ToolMode::kImageGen),
      nullptr);
  EXPECT_THAT(actual, ElementsAre(Eq(std::cref(chip0)), Eq(std::cref(chip1))));
}

TEST(ActionChipGeneratorTest, NewEndpointOptOutFallsBackToStaticOnFailure) {
  EnvironmentFixture env;
  const GURL page_url("https://www.google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  EXPECT_CALL(generator_fixture.fake_client(),
              IsPersonalizedUrlDataCollectionActive())
      .WillOnce(Return(false));

  // Should call remote service with nullopt title and URL, and fail.
  EXPECT_CALL(generator_fixture.mock_service(),
              GetActionChipSuggestions(Eq(std::nullopt), Eq(std::nullopt), _,
                                       Eq(std::nullopt), _))
      .WillOnce(WithArg<4>(
          [](base::OnceCallback<void(
                 RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
                 callback) {
            std::move(callback).Run(
                base::unexpected(RemoteSuggestionsServiceSimple::NetworkError{
                    .net_error = net::ERR_TIMED_OUT}));
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpNextFeatures,
        {{ntp_features::kNtpNextShowStaticTextParam.name, "false"}}},
       {ntp_features::kNtpNextCanvasChip, {}}},
      {});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  // Expect static chips (without recent tab chip because opted out).
  EXPECT_THAT(actual, ElementsAre(Eq(std::cref(GetStaticImageGenerationChip())),
                                  Eq(std::cref(GetStaticCanvasChip())),
                                  Eq(std::cref(GetStaticDeepSearchChip()))));
}

TEST(ActionChipGeneratorTest, NewEndpointEmptyResponseReturnsEmptyChips) {
  EnvironmentFixture env;
  base::HistogramTester histogram_tester;
  const GURL page_url("https://www.google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  EXPECT_CALL(generator_fixture.mock_service(),
              GetActionChipSuggestions(Eq(page_title), Eq(page_url), _, _, _))
      .WillOnce(WithArg<4>(
          [](base::OnceCallback<void(
                 RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
                 callback) {
            std::move(callback).Run(SearchSuggestionParser::SuggestResults{});
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  EXPECT_TRUE(actual.empty());
  histogram_tester.ExpectUniqueSample("NewTabPage.ActionChips.RequestStatus",
                                      ActionChipsRequestStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample("NewTabPage.ActionChips.SuggestionCount",
                                      0, 1);
}

TEST(ActionChipGeneratorTest, NewEndpointParseErrorFallsBackToStaticChips) {
  EnvironmentFixture env;
  base::HistogramTester histogram_tester;
  const GURL page_url("https://www.google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  EXPECT_CALL(generator_fixture.mock_service(),
              GetActionChipSuggestions(Eq(page_title), Eq(page_url), _, _, _))
      .WillOnce(WithArg<4>(
          [](base::OnceCallback<void(
                 RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
                 callback) {
            std::move(callback).Run(
                base::unexpected(RemoteSuggestionsServiceSimple::ParseError{
                    .parse_failure_reason = RemoteSuggestionsServiceSimple::
                        ParseFailureReason::kMalformedJson}));
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpNextFeatures,
        {{ntp_features::kNtpNextShowStaticTextParam.name, "false"}}},
       {ntp_features::kNtpNextCanvasChip, {}}},
      {});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  EXPECT_THAT(actual, ElementsAre(Eq(std::cref(GetStaticImageGenerationChip())),
                                  Eq(std::cref(GetStaticCanvasChip())),
                                  Eq(std::cref(GetStaticDeepSearchChip()))));

  histogram_tester.ExpectUniqueSample("NewTabPage.ActionChips.RequestStatus",
                                      ActionChipsRequestStatus::kParseError, 1);
  histogram_tester.ExpectUniqueSample(
      "NewTabPage.ActionChips.RequestStatus.ParseError",
      RemoteSuggestionsServiceSimple::ParseFailureReason::kMalformedJson, 1);
}

TEST(ActionChipGeneratorTest, NewEndpointPartialEligibilityPassesCorrectTools) {
  EnvironmentFixture env;
  const GURL page_url("https://www.google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  // Only Deep Search is eligible.
  generator_fixture.set_searchbox_config({TOOL_MODE_DEEP_SEARCH});

  EXPECT_CALL(
      generator_fixture.mock_service(),
      GetActionChipSuggestions(Eq(page_title), Eq(page_url),
                               // Expect ONLY Deep Search tool mode.
                               ElementsAre(omnibox::TOOL_MODE_DEEP_SEARCH),
                               Eq(std::nullopt), _))
      .WillOnce(WithArg<4>(
          [](base::OnceCallback<void(
                 RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
                 callback) {
            std::move(callback).Run(SearchSuggestionParser::SuggestResults{});
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();
}

TEST(ActionChipGeneratorTest, NewEndpointFiltersInvalidSuggestions) {
  EnvironmentFixture env;
  const GURL page_url("https://www.google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  EXPECT_CALL(generator_fixture.mock_service(),
              GetActionChipSuggestions(Eq(page_title), Eq(page_url), _, _, _))
      .WillOnce(WithArg<4>([&](base::OnceCallback<void(
                                   RemoteSuggestionsServiceSimple::
                                       ActionChipSuggestionsResult&&)>
                                   callback) {
        SearchSuggestionParser::SuggestResults results;

        // Valid suggestion.
        results.push_back(CreateSuggestion(
            {.group_id = omnibox::GROUP_AI_MODE_DEEP_SEARCH_ACTION,
             .icon_type = omnibox::SuggestTemplateInfo::GLOBE_WITH_SEARCH_LOOP,
             .match_contents = "Valid Title",
             .annotation = "Valid Annotation",
             .primary_a11y_text = "Primary A11y Text",
             .secondary_a11y_text = "Secondary A11y Text"}));

        // Invalid suggestion: SuggestType != TYPE_FUSEBOX_ACTION.
        SearchSuggestionParser::SuggestResult no_fusebox_result =
            CreateSuggestion(
                {.group_id = omnibox::GROUP_AI_MODE_DEEP_SEARCH_ACTION,
                 .match_contents = "Title",
                 .annotation = "Annotation",
                 .suggest_type = omnibox::SuggestType::TYPE_QUERY});
        omnibox::SuggestTemplateInfo valid_icon_info;
        valid_icon_info.set_type_icon(omnibox::SuggestTemplateInfo::FAVICON);
        no_fusebox_result.SetSuggestTemplateInfo(valid_icon_info);
        results.push_back(std::move(no_fusebox_result));

        // Invalid suggestion: Missing SuggestTemplateInfo.
        SearchSuggestionParser::SuggestResult no_template_info_result =
            CreateSuggestion(
                {.icon_type = omnibox::SuggestTemplateInfo::FAVICON,
                 .match_contents = "Title",
                 .annotation = "Annotation"});
        no_template_info_result.set_suggestion_group_id(
            omnibox::GROUP_AI_MODE_DEEP_SEARCH_ACTION);
        results.push_back(std::move(no_template_info_result));

        // Invalid suggestion: Unspecified IconType.
        results.push_back(CreateSuggestion(
            {.group_id = omnibox::GROUP_AI_MODE_DEEP_SEARCH_ACTION,
             .icon_type = omnibox::SuggestTemplateInfo::ICON_TYPE_UNSPECIFIED,
             .match_contents = "Title",
             .annotation = "Annotation"}));

        std::move(callback).Run(std::move(results));
        return nullptr;
      }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  ActionChipPtr valid_chip = ActionChip::New();
  valid_chip->suggest_template_info = SuggestTemplateInfo::New();
  valid_chip->suggest_template_info->type_icon = IconType::kGlobeWithSearchLoop;
  valid_chip->suggest_template_info->primary_text =
      CreateFormattedString("Valid Title", "Primary A11y Text");
  valid_chip->suggest_template_info->secondary_text =
      CreateFormattedString("Valid Annotation", "Secondary A11y Text");
  valid_chip->suggest_template_info->preselected_tool = ToolMode::kUnspecified;

  // Expect only the valid chip.
  EXPECT_THAT(actual, ElementsAre(Eq(std::cref(valid_chip))));
}

TEST(ActionChipGeneratorTest, StaticChipsParamTakesPrecedenceOverNewEndpoint) {
  EnvironmentFixture env;
  const GURL page_url("https://www.google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  // Even though new endpoint is enabled, we should NOT call the service.
  EXPECT_CALL(generator_fixture.mock_service(), GetActionChipSuggestions)
      .Times(0);

  base::test::ScopedFeatureList list;
  list.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpNextFeatures,
        {{ntp_features::kNtpNextShowStaticTextParam.name, "true"}}},
       {ntp_features::kNtpNextCanvasChip, {}}},
      {});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  // Expect static chips.
  EXPECT_THAT(actual, ElementsAre(Eq(std::cref(GetStaticImageGenerationChip())),
                                  Eq(std::cref(GetStaticCanvasChip())),
                                  Eq(std::cref(GetStaticDeepSearchChip()))));
}

TEST(ActionChipGeneratorTest,
     NoRecentTabChipWhenNtpNextShowStaticRecentTabChipParamIsFalse) {
  EnvironmentFixture env;
  const GURL page_url("https://www.google.com/");
  const std::u16string page_title(u"Some Title");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  base::test::ScopedFeatureList list;
  list.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpNextFeatures,
        {{ntp_features::kNtpNextShowStaticTextParam.name, "true"},
         {ntp_features::kNtpNextShowStaticRecentTabChipParam.name, "false"}}}},
      {ntp_features::kNtpNextCanvasChip});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
}

}  // namespace
