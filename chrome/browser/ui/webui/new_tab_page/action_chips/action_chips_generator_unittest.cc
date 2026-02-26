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
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_metrics.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_mojo_test_utils.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/fake_tab_id_generator.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/remote_suggestions_service_simple.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/proto/hints.pb.h"
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
                               CreateFormattedString(subtitle)),
      std::move(tab));
}

const ActionChipPtr& GetStaticDeepSearchChip() {
  static const base::NoDestructor<ActionChipPtr> kInstance(CreateActionChip(
      /*suggestion=*/"",
      SuggestTemplateInfo::New(
          IconType::kGlobeWithSearchLoop, CreateFormattedString("Deep Search"),
          CreateFormattedString("Dive deep into something new")),
      /*tab=*/nullptr));
  return *kInstance;
}

const ActionChipPtr& GetStaticImageGenerationChip() {
  static const base::NoDestructor<ActionChipPtr> kInstance(CreateActionChip(
      /*suggestion=*/"",
      SuggestTemplateInfo::New(
          IconType::kBanana, CreateFormattedString("Create images"),
          CreateFormattedString("Add an image and reimagine it")),
      /*tab=*/nullptr));
  return *kInstance;
}

ActionChipPtr CreateStaticDeepDiveChip(TabInfoPtr tab,
                                       std::string_view suggestion) {
  return CreateActionChip(
      std::string(suggestion),
      SuggestTemplateInfo::New(IconType::kSubArrowRight,
                               /*primary_text=*/nullptr,
                               CreateFormattedString(std::string(suggestion))),
      std::move(tab));
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
        FakeTabIdGenerator::Get(), &mock_optimization_guide_,
        mock_aim_eligibility_service_.get(), std::move(client),
        std::move(service));

    ON_CALL(*fake_client_, IsPersonalizedUrlDataCollectionActive())
        .WillByDefault(Return(true));

    set_searchbox_config({TOOL_MODE_DEEP_SEARCH, TOOL_MODE_IMAGE_GEN});
    ON_CALL(*mock_aim_eligibility_service_, IsDeepSearchEligible)
        .WillByDefault(Return(true));
    ON_CALL(*mock_aim_eligibility_service_, IsCreateImagesEligible)
        .WillByDefault(Return(true));
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

  MockOptimizationGuideKeyedService& mock_optimization_guide() {
    return mock_optimization_guide_;
  }

  MockAimEligibilityService& mock_aim_eligibility_service() {
    return *mock_aim_eligibility_service_;
  }

  void set_searchbox_config(base::span<const omnibox::ToolMode> allowed_tools) {
    mock_aim_eligibility_service_->config()
        .mutable_rule_set()
        ->mutable_allowed_tools()
        ->Assign(allowed_tools.begin(), allowed_tools.end());
  }

  // Makes the optimization guide's mock permissive. i.e., after the call to
  // this method, the mock considers any URL as EDU vertical.
  void MakeOptimizationGuidePermissive() {
    EXPECT_CALL(
        mock_optimization_guide_,
        CanApplyOptimization(
            _, _, TypedEq<optimization_guide::OptimizationMetadata*>(nullptr)))
        .Times(AnyNumber())
        .WillRepeatedly(
            Return(optimization_guide::OptimizationGuideDecision::kTrue));
  }

 private:
  // generator_ must be declared first so raw_ptr's check does not detect
  // the use-after-free issue.
  testing::StrictMock<MockOptimizationGuideKeyedService>
      mock_optimization_guide_;
  TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<MockAimEligibilityService> mock_aim_eligibility_service_;
  std::unique_ptr<ActionChipsGeneratorImpl> generator_;
  raw_ptr<FakeAutocompleteProviderClient> fake_client_ = nullptr;
  raw_ptr<MockRemoteSuggestionsServiceSimple> mock_service_ = nullptr;
};

using ActionChipGeneratorWithNoRecentTabTest = ::testing::TestWithParam<bool>;

INSTANTIATE_TEST_SUITE_P(ActionChipGeneratorTests,
                         ActionChipGeneratorWithNoRecentTabTest,
                         ::testing::Bool());

TEST_P(ActionChipGeneratorWithNoRecentTabTest,
       GenerateTwoStaticChipsWhenNoTabIsPassed) {
  EnvironmentFixture env;
  GeneratorFixture generator_fixture;
  base::RunLoop run_loop;
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name,
        GetParam() ? "true" : "false"}});

  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(std::nullopt, run_loop, actual);
  run_loop.Run();
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
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
  ActionChipPtr most_recent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
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
  using TupleT = std::tuple<bool, bool, bool>;
  explicit StaticChipsGenerationWithAimEligibilityTestCase(TupleT tuple)
      : tab_exists(get<0>(tuple)),
        is_deepsearch_eligible(get<1>(tuple)),
        is_create_images_eligible(get<2>(tuple)) {}
};

using ActionChipGeneratorStaticChipsGenerationWithAimEligibilityTest =
    testing::TestWithParam<StaticChipsGenerationWithAimEligibilityTestCase>;
INSTANTIATE_TEST_SUITE_P(
    ActionChipGeneratorTests,
    ActionChipGeneratorStaticChipsGenerationWithAimEligibilityTest,
    ::testing::ConvertGenerator<
        StaticChipsGenerationWithAimEligibilityTestCase::TupleT>(
        testing::Combine(testing::Bool(), testing::Bool(), testing::Bool())),
    [](const testing::TestParamInfo<
        StaticChipsGenerationWithAimEligibilityTestCase>& param_info) {
      const StaticChipsGenerationWithAimEligibilityTestCase& param =
          param_info.param;
      std::string test_name;
      if (!param.is_deepsearch_eligible && !param.is_create_images_eligible) {
        test_name = "NoEligibility";
      } else {
        test_name = "EligibilityFor";
        if (param.is_deepsearch_eligible) {
          test_name += "DeepSearch";
        }
        if (param.is_create_images_eligible) {
          test_name += "ImageCreation";
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
  const GURL page_url("https://google.com/");
  const std::u16string page_title(u"Google");
  std::optional<TabFixture> tab_fixture;
  if (GetParam().tab_exists) {
    tab_fixture.emplace(page_url, page_title);
  }
  GeneratorFixture generator_fixture;

  EXPECT_CALL(generator_fixture.mock_aim_eligibility_service(),
              IsDeepSearchEligible())
      .WillOnce(Return(GetParam().is_deepsearch_eligible));
  EXPECT_CALL(generator_fixture.mock_aim_eligibility_service(),
              IsCreateImagesEligible())
      .WillOnce(Return(GetParam().is_create_images_eligible));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  TabInterface* tab =
      tab_fixture.has_value() ? &tab_fixture->mock_tab() : nullptr;
  generator_fixture.GenerateActionChips(tab, run_loop, actual);
  run_loop.Run();

  std::vector<Matcher<ActionChipPtr>> expected;
  ActionChipPtr most_recent_tab_chip;
  if (tab != nullptr) {
    most_recent_tab_chip = CreateStaticRecentTabChip(CreateTabInfo(tab));
    expected.push_back(Eq(std::cref(most_recent_tab_chip)));
  }
  if (GetParam().is_deepsearch_eligible) {
    expected.push_back(Eq(std::cref(GetStaticDeepSearchChip())));
  }
  if (GetParam().is_create_images_eligible) {
    expected.push_back(Eq(std::cref(GetStaticImageGenerationChip())));
  }
  EXPECT_THAT(actual, ElementsAreArray(expected));
}

TEST(ActionChipGeneratorWithNoRecentTabTest,
     GenerateStaticChipsWhenHistorySyncIsOptOut) {
  EnvironmentFixture env;
  const GURL page_url("https://google.com/");
  const std::u16string page_title(u"Google");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;
  EXPECT_CALL(generator_fixture.fake_client(),
              IsPersonalizedUrlDataCollectionActive())
      .WillOnce(Return(false));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();
  ActionChipPtr most_recent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_recent_tab_chip)),
                          Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
}

struct CanApplyOptimizationCall {
  optimization_guide::proto::OptimizationType type =
      optimization_guide::proto::TYPE_UNSPECIFIED;
  optimization_guide::OptimizationGuideDecision ret_val =
      optimization_guide::OptimizationGuideDecision::kUnknown;
};

struct DeepDiveTestParam {
  std::string test_name;
  // Whether the deep dive param is enabled.
  bool deep_dive_param_enabled;
  // A series of calls to CanApplyOptimization.
  std::vector<CanApplyOptimizationCall> calls;
  // Whether we expect deep dive chips to be shown.
  bool expect_deep_dive;
};

using ActionChipsGeneratorDeepDiveTest =
    testing::TestWithParam<DeepDiveTestParam>;

INSTANTIATE_TEST_SUITE_P(
    ActionChipGeneratorTests,
    ActionChipsGeneratorDeepDiveTest,
    ::testing::ValuesIn(
        {// The param is enabled and the URL is in the deep dive vertical
         // (not on blocklist, on allowlist).
         DeepDiveTestParam{
             .test_name = "UrlIsInDeepDiveVerticalAndAllowed",
             .deep_dive_param_enabled = true,
             .calls = {{optimization_guide::proto::
                            NTP_NEXT_DEEP_DIVE_ACTION_CHIP_BLOCKLIST,
                        optimization_guide::OptimizationGuideDecision::kTrue},
                       {optimization_guide::proto::
                            NTP_NEXT_DEEP_DIVE_ACTION_CHIP_ALLOWLIST,
                        optimization_guide::OptimizationGuideDecision::kTrue}},
             .expect_deep_dive = true},
         // The param is enabled, URL is not on blocklist, but not on
         // allowlist.
         {.test_name = "UrlIsOnNeitherAllowListNorBlockList",
          .deep_dive_param_enabled = true,
          .calls = {{optimization_guide::proto::
                         NTP_NEXT_DEEP_DIVE_ACTION_CHIP_BLOCKLIST,
                     optimization_guide::OptimizationGuideDecision::kTrue},
                    {optimization_guide::proto::
                         NTP_NEXT_DEEP_DIVE_ACTION_CHIP_ALLOWLIST,
                     optimization_guide::OptimizationGuideDecision::kFalse}},
          .expect_deep_dive = false},
         // The param is enabled, URL is on allowlist, but also on blocklist.
         {.test_name = "UrlIsBothOnAllowListAndBlockList",
          .deep_dive_param_enabled = true,
          .calls = {{optimization_guide::proto::
                         NTP_NEXT_DEEP_DIVE_ACTION_CHIP_BLOCKLIST,
                     optimization_guide::OptimizationGuideDecision::kFalse},
                    {optimization_guide::proto::
                         NTP_NEXT_DEEP_DIVE_ACTION_CHIP_ALLOWLIST,
                     optimization_guide::OptimizationGuideDecision::kTrue}},
          .expect_deep_dive = false},
         // The param is enabled, URL is on blocklist and not on allowlist.
         {.test_name = "UrlIsOnlyOnBlockList",
          .deep_dive_param_enabled = true,
          .calls = {{optimization_guide::proto::
                         NTP_NEXT_DEEP_DIVE_ACTION_CHIP_BLOCKLIST,
                     optimization_guide::OptimizationGuideDecision::kFalse},
                    {optimization_guide::proto::
                         NTP_NEXT_DEEP_DIVE_ACTION_CHIP_ALLOWLIST,
                     optimization_guide::OptimizationGuideDecision::kFalse}},
          .expect_deep_dive = false},
         // The param is disabled.
         {.test_name = "DeepDiveChipsAreDisabled",
          .deep_dive_param_enabled = false,
          .calls = {},
          .expect_deep_dive = false}}),
    [](const testing::TestParamInfo<DeepDiveTestParam>& param) {
      return param.param.test_name;
    });

TEST_P(ActionChipsGeneratorDeepDiveTest, GenerateChips) {
  EnvironmentFixture env;
  const GURL page_url("https://en.wikipedia.org/wiki/Mathematics");
  const std::u16string page_title(u"Mathematics - Wikipedia");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;
  EXPECT_CALL(generator_fixture.mock_service(),
              GetDeepdiveChipSuggestionsForTab(Eq(page_title), Eq(page_url), _))
      .Times(GetParam().expect_deep_dive ? 1 : 0)
      .WillOnce(WithArg<2>(
          [](base::OnceCallback<void(
                 RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
                 callback) {
            std::move(callback).Run(SearchSuggestionParser::SuggestResults{
                CreateSuggestion({.match_contents = "Test suggestion 1",
                                  .suggestion = u"Test suggestion 1"}),
                CreateSuggestion({.match_contents = "Test suggestion 3",
                                  .suggestion = u"Test suggestion 3"})});
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextShowDeepDiveSuggestionsParam.name,
        GetParam().deep_dive_param_enabled ? "true" : "false"}});

  for (const auto& call : GetParam().calls) {
    EXPECT_CALL(
        generator_fixture.mock_optimization_guide(),
        CanApplyOptimization(
            page_url, call.type,
            TypedEq<optimization_guide::OptimizationMetadata*>(nullptr)))
        .WillOnce(Return(call.ret_val));
  }

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();
  const TabInfoPtr tab_info = CreateTabInfo(&tab_fixture.mock_tab());
  ActionChipPtr most_recent_tab_chip =
      CreateStaticRecentTabChip(tab_info->Clone());
  ActionChipPtr deep_dive_chip_1 =
      CreateStaticDeepDiveChip(tab_info->Clone(), "Test suggestion 1");
  ActionChipPtr deep_dive_chip_2 =
      CreateStaticDeepDiveChip(tab_info->Clone(), "Test suggestion 3");

  EXPECT_THAT(actual,
              testing::Conditional(
                  GetParam().expect_deep_dive,
                  ElementsAre(Eq(std::cref(most_recent_tab_chip)),
                              Eq(std::cref(deep_dive_chip_1)),
                              Eq(std::cref(deep_dive_chip_2))),
                  ElementsAre(Eq(std::cref(most_recent_tab_chip)),
                              Eq(std::cref(GetStaticDeepSearchChip())),
                              Eq(std::cref(GetStaticImageGenerationChip())))));
}

TEST(ActionChipGeneratorTest,
     DeepDiveChipGenerationFallsBackToStaticChipsWhenRemoteCallFails) {
  EnvironmentFixture env;
  const GURL page_url("https://en.wikipedia.org/wiki/Mathematics");
  const std::u16string page_title(u"Mathematics - Wikipedia");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;
  generator_fixture.MakeOptimizationGuidePermissive();

  EXPECT_CALL(generator_fixture.mock_service(),
              GetDeepdiveChipSuggestionsForTab(Eq(page_title), Eq(page_url), _))
      .WillOnce(WithArg<2>(
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
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextShowDeepDiveSuggestionsParam.name, "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  ActionChipPtr most_recent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_recent_tab_chip)),
                          Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
}

TEST(ActionChipGeneratorTest,
     DeepDiveChipGenerationFallsBackToStaticChipsWhenRemoteCallIsEmpty) {
  EnvironmentFixture env;
  const GURL page_url("https://en.wikipedia.org/wiki/Mathematics");
  const std::u16string page_title(u"Mathematics - Wikipedia");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;
  generator_fixture.MakeOptimizationGuidePermissive();

  EXPECT_CALL(generator_fixture.mock_service(),
              GetDeepdiveChipSuggestionsForTab(Eq(page_title), Eq(page_url), _))
      .WillOnce(WithArg<2>(
          [](base::OnceCallback<void(
                 RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
                 callback) {
            std::move(callback).Run(SearchSuggestionParser::SuggestResults{});
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextShowDeepDiveSuggestionsParam.name, "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  ActionChipPtr most_recent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_recent_tab_chip)),
                          Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
}

TEST(ActionChipGeneratorTest,
     DeepDiveChipGenerationFallsBackToStaticChipsWhenRemoteCallIsOne) {
  EnvironmentFixture env;
  const GURL page_url("https://en.wikipedia.org/wiki/Mathematics");
  const std::u16string page_title(u"Mathematics - Wikipedia");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;
  generator_fixture.MakeOptimizationGuidePermissive();

  EXPECT_CALL(generator_fixture.mock_service(),
              GetDeepdiveChipSuggestionsForTab(Eq(page_title), Eq(page_url), _))
      .WillOnce(WithArg<2>(
          [](base::OnceCallback<void(
                 RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
                 callback) {
            std::move(callback).Run(SearchSuggestionParser::SuggestResults{
                CreateSuggestion({.match_contents = "Test suggestion 1",
                                  .suggestion = u"Test suggestion 1"})});
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextShowDeepDiveSuggestionsParam.name, "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  ActionChipPtr most_recent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_recent_tab_chip)),
                          Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
}

TEST(ActionChipGeneratorTest, DeepDiveWithNewEndpoint) {
  EnvironmentFixture env;
  const GURL page_url("https://en.wikipedia.org/wiki/Mathematics");
  const std::u16string page_title(u"Mathematics - Wikipedia");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;
  generator_fixture.MakeOptimizationGuidePermissive();

  const std::string recent_tab_title = "Ask about previous tab";
  const std::string recent_tab_subtitle = "Subtitle for recent tab";
  const std::u16string recent_tab_suggestion = u"Suggestion for recent tab";
  const std::string deep_dive_title_1 = "Solve the equations";
  const std::string deep_dive_subtitle_1 = "Subtitle for deep dive 1";
  const std::u16string deep_dive_suggestion_1 = u"Suggestion for deep dive 1";
  const std::string deep_dive_title_2 = "Explain the steps involved";
  const std::string deep_dive_subtitle_2 = "Subtitle for deep dive 2";
  const std::u16string deep_dive_suggestion_2 = u"Suggestion for deep dive 2";

  EXPECT_CALL(generator_fixture.mock_service(),
              GetActionChipSuggestions(
                  Eq(page_title), Eq(page_url),
                  ElementsAre(omnibox::TOOL_MODE_DEEP_SEARCH,
                              omnibox::TOOL_MODE_IMAGE_GEN),
                  TypedEq<base::optional_ref<const omnibox::PageVertical>>(
                      omnibox::PageVertical::PAGE_VERTICAL_EDU),
                  _))
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
                {.group_id = omnibox::GROUP_AI_MODE_CONTEXTUAL_SEARCH_ACTION,
                 .icon_type = omnibox::SuggestTemplateInfo::SUB_ARROW_RIGHT,
                 .match_contents = deep_dive_title_1,
                 .annotation = deep_dive_subtitle_1,
                 .suggestion = deep_dive_suggestion_1}),
            CreateSuggestion(
                {.group_id = omnibox::GROUP_AI_MODE_CONTEXTUAL_SEARCH_ACTION,
                 .icon_type = omnibox::SuggestTemplateInfo::SUB_ARROW_RIGHT,
                 .match_contents = deep_dive_title_2,
                 .annotation = deep_dive_subtitle_2,
                 .suggestion = deep_dive_suggestion_2})});
        return nullptr;
      }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextShowDeepDiveSuggestionsParam.name, "true"},
       {ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
            .name,
        "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  TabInfoPtr tab_info = CreateTabInfo(&tab_fixture.mock_tab());
  ActionChipPtr chip0 = CreateActionChip(
      base::UTF16ToUTF8(recent_tab_suggestion),
      SuggestTemplateInfo::New(IconType::kFavicon,
                               CreateFormattedString(recent_tab_title),
                               CreateFormattedString(recent_tab_subtitle)),
      tab_info->Clone());
  ActionChipPtr chip1 = CreateActionChip(
      base::UTF16ToUTF8(deep_dive_suggestion_1),
      SuggestTemplateInfo::New(IconType::kSubArrowRight,
                               CreateFormattedString(deep_dive_title_1),
                               CreateFormattedString(deep_dive_subtitle_1)),
      tab_info->Clone());
  ActionChipPtr chip2 = CreateActionChip(
      base::UTF16ToUTF8(deep_dive_suggestion_2),
      SuggestTemplateInfo::New(IconType::kSubArrowRight,
                               CreateFormattedString(deep_dive_title_2),
                               CreateFormattedString(deep_dive_subtitle_2)),
      tab_info->Clone());

  EXPECT_THAT(actual, ElementsAre(Eq(std::cref(chip0)), Eq(std::cref(chip1)),
                                  Eq(std::cref(chip2))));
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
      .WillOnce(WithArg<4>(
          [&](base::OnceCallback<void(RemoteSuggestionsServiceSimple::
                                          ActionChipSuggestionsResult&&)>
                  callback) {
            std::move(callback).Run(SearchSuggestionParser::SuggestResults{
                CreateSuggestion(
                    {.group_id =
                         omnibox::GROUP_AI_MODE_CONTEXTUAL_SEARCH_ACTION,
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
                     .suggestion = deep_search_suggestion}),
                CreateSuggestion(
                    {.group_id = omnibox::GROUP_AI_MODE_CREATE_IMAGE_ACTION,
                     .icon_type = omnibox::SuggestTemplateInfo::BANANA,
                     .match_contents = image_gen_title,
                     .annotation = image_gen_subtitle,
                     .suggestion = image_gen_suggestion})});
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
            .name,
        "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  TabInfoPtr tab_info = CreateTabInfo(&tab_fixture.mock_tab());
  ActionChipPtr chip0 = CreateActionChip(
      base::UTF16ToUTF8(recent_tab_suggestion),
      SuggestTemplateInfo::New(IconType::kFavicon,
                               CreateFormattedString(recent_tab_title),
                               CreateFormattedString(recent_tab_subtitle)),
      tab_info->Clone());
  ActionChipPtr chip1 = CreateActionChip(
      base::UTF16ToUTF8(deep_search_suggestion),
      SuggestTemplateInfo::New(IconType::kGlobeWithSearchLoop,
                               CreateFormattedString(deep_search_title),
                               CreateFormattedString(deep_search_subtitle)),
      nullptr);
  ActionChipPtr chip2 = CreateActionChip(
      base::UTF16ToUTF8(image_gen_suggestion),
      SuggestTemplateInfo::New(IconType::kBanana,
                               CreateFormattedString(image_gen_title),
                               CreateFormattedString(image_gen_subtitle)),
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
                     .suggestion = deep_search_suggestion}),
                CreateSuggestion(
                    {.group_id = omnibox::GROUP_AI_MODE_CREATE_IMAGE_ACTION,
                     .icon_type = omnibox::SuggestTemplateInfo::BANANA,
                     .match_contents = image_gen_title,
                     .annotation = image_gen_subtitle,
                     .suggestion = image_gen_suggestion})});
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
            .name,
        "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(std::nullopt, run_loop, actual);
  run_loop.Run();

  ActionChipPtr chip0 = CreateActionChip(
      base::UTF16ToUTF8(deep_search_suggestion),
      SuggestTemplateInfo::New(IconType::kGlobeWithSearchLoop,
                               CreateFormattedString(deep_search_title),
                               CreateFormattedString(deep_search_subtitle)),
      nullptr);
  ActionChipPtr chip1 = CreateActionChip(
      base::UTF16ToUTF8(image_gen_suggestion),
      SuggestTemplateInfo::New(IconType::kBanana,
                               CreateFormattedString(image_gen_title),
                               CreateFormattedString(image_gen_subtitle)),
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
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
            .name,
        "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  ActionChipPtr most_recent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_recent_tab_chip)),
                          Eq(std::cref(GetStaticDeepSearchChip())),
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
                     .suggestion = base::UTF8ToUTF16(deep_search_suggestion)}),
                CreateSuggestion(
                    {.group_id = omnibox::GROUP_AI_MODE_CREATE_IMAGE_ACTION,
                     .icon_type = omnibox::SuggestTemplateInfo::BANANA,
                     .match_contents = image_gen_title,
                     .annotation = image_gen_subtitle,
                     .suggestion = base::UTF8ToUTF16(image_gen_suggestion)})});
            return nullptr;
          }));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
            .name,
        "true"}});

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
                               CreateFormattedString(deep_search_subtitle)),
      nullptr);
  ActionChipPtr chip1 = CreateActionChip(
      image_gen_suggestion,
      SuggestTemplateInfo::New(IconType::kBanana,
                               CreateFormattedString(image_gen_title),
                               CreateFormattedString(image_gen_subtitle)),
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
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
            .name,
        "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  // Expect static chips (without recent tab chip because opted out).
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
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
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
            .name,
        "true"}});

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
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
            .name,
        "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  ActionChipPtr most_recent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_recent_tab_chip)),
                          Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));

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
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
            .name,
        "true"}});

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
      {{ntp_features::kNtpNextShowStaticTextParam.name, "false"},
       {ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
            .name,
        "true"}});

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
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "true"},
       {ntp_features::kNtpNextSuggestionsFromNewSearchSuggestionsEndpointParam
            .name,
        "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  // Expect static chips.
  ActionChipPtr most_recent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_recent_tab_chip)),
                          Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
}

TEST(ActionChipGeneratorTest,
     GenerateSimplifiedRecentTabChipWhenSimplificationUIParamIsTrue) {
  EnvironmentFixture env;
  const GURL page_url("https://www.google.com/");
  const std::u16string page_title(u"Some Title");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "true"},
       {ntp_features::kNtpNextShowSimplificationUIParam.name, "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  generator_fixture.GenerateActionChips(&tab_fixture.mock_tab(), run_loop,
                                        actual);
  run_loop.Run();

  TabInfoPtr tab_info = CreateTabInfo(&tab_fixture.mock_tab());
  ActionChipPtr expected_recent_tab_chip = ActionChip::New(
      /*suggestion=*/"",
      SuggestTemplateInfo::New(IconType::kFavicon,
                               CreateFormattedString("Ask about previous tab"),
                               CreateFormattedString("Some Title")),
      /*tab=*/tab_info->Clone());

  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(expected_recent_tab_chip)),
                          Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
}

TEST(ActionChipGeneratorTest,
     NoRecentTabChipWhenNtpNextShowStaticRecentTabChipParamIsFalse) {
  EnvironmentFixture env;
  const GURL page_url("https://www.google.com/");
  const std::u16string page_title(u"Some Title");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "true"},
       {ntp_features::kNtpNextShowStaticRecentTabChipParam.name, "false"}});

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
