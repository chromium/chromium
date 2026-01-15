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
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-data-view.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
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
#include "url/gurl.h"
namespace {
using ::action_chips::RemoteSuggestionsServiceSimple;
using ::action_chips::mojom::ActionChip;
using ::action_chips::mojom::ActionChipPtr;
using ::action_chips::mojom::ChipType;
using ::action_chips::mojom::TabInfo;
using ::action_chips::mojom::TabInfoPtr;
using ::sync_preferences::TestingPrefServiceSyncable;
using ::tabs::TabInterface;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Matcher;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::TypedEq;
using ::testing::WithArg;

struct SuggestResultFields {
  std::u16string suggestion;
  AutocompleteMatchType::Type type =
      AutocompleteMatchType::Type::SEARCH_SUGGEST;
  omnibox::SuggestType suggest_type = omnibox::SuggestType::TYPE_QUERY;
  std::vector<int> subtypes;
  bool from_keyword = false;
  omnibox::NavigationalIntent navigational_intent =
      omnibox::NavigationalIntent::NAV_INTENT_NONE;
  int relevance = 100;
  bool relevance_from_server = true;
  std::u16string input_text = u"";
};

SearchSuggestionParser::SuggestResult MakeResult(
    const SuggestResultFields& fields,
    std::optional<omnibox::GroupId> group_id = std::nullopt) {
  auto result = SearchSuggestionParser::SuggestResult(
      fields.suggestion, fields.type, fields.suggest_type, fields.subtypes,
      fields.from_keyword, fields.navigational_intent, fields.relevance,
      fields.relevance_from_server, fields.input_text);
  if (group_id.has_value()) {
    result.set_suggestion_group_id(group_id.value());
  }
  return result;
}

class MockRemoteSuggestionsServiceSimple
    : public RemoteSuggestionsServiceSimple {
 public:
  MockRemoteSuggestionsServiceSimple() = default;
  ~MockRemoteSuggestionsServiceSimple() override = default;

  MOCK_METHOD(std::unique_ptr<network::SimpleURLLoader>,
              GetActionChipSuggestionsForTab,
              (const std::u16string_view title,
               const GURL& url,
               base::OnceCallback<void(ActionChipSuggestionsResult&&)>),
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
  return ActionChip::New(title, tab->title, ChipType::kRecentTab,
                         std::move(tab));
}

const ActionChipPtr& GetStaticDeepSearchChip() {
  static const base::NoDestructor<ActionChipPtr> kInstance(ActionChip::New(
      /*title=*/"Deep Search",
      /*suggestion=*/"Dive deep into something new",
      /*type=*/ChipType::kDeepSearch, /*tab=*/nullptr));
  return *kInstance;
}

const ActionChipPtr& GetStaticImageGenerationChip() {
  static const base::NoDestructor<ActionChipPtr> kInstance(ActionChip::New(
      /*title=*/"Create images",
      /*suggestion=*/"Add an image and reimagine it",
      /*type=*/ChipType::kImage, /*tab=*/nullptr));
  return *kInstance;
}

ActionChipPtr CreateStaticDeepDiveChip(TabInfoPtr tab,
                                       std::string_view suggestion) {
  return ActionChip::New(/*title=*/"", std::string(suggestion),
                         ChipType::kDeepDive, std::move(tab));
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
        pref_service_, nullptr, nullptr, nullptr, false);

    generator_ = std::make_unique<ActionChipsGeneratorImpl>(
        FakeTabIdGenerator::Get(), &mock_optimization_guide_,
        mock_aim_eligibility_service_.get(), std::move(client),
        std::move(service));

    ON_CALL(*fake_client_, IsPersonalizedUrlDataCollectionActive())
        .WillByDefault(Return(true));
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
  ActionChipPtr most_resent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_resent_tab_chip)),
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
  ActionChipPtr most_resent_tab_chip;
  if (tab != nullptr) {
    most_resent_tab_chip = CreateStaticRecentTabChip(CreateTabInfo(tab));
    expected.push_back(Eq(std::cref(most_resent_tab_chip)));
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
  ActionChipPtr most_resent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_resent_tab_chip)),
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
              GetActionChipSuggestionsForTab(Eq(page_title), Eq(page_url), _))
      .Times(GetParam().expect_deep_dive ? 1 : 0)
      .WillOnce(WithArg<2>(
          [](base::OnceCallback<void(
                 RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&&)>
                 callback) {
            std::move(callback).Run(SearchSuggestionParser::SuggestResults{
                MakeResult({.suggestion = u"Test suggestion 1"}),
                MakeResult({.suggestion = u"Test suggestion 2"},
                           omnibox::GroupId::GROUP_PERSONALIZED_ZERO_SUGGEST),
                MakeResult({.suggestion = u"Test suggestion 3"})});
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
              GetActionChipSuggestionsForTab(Eq(page_title), Eq(page_url), _))
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

  ActionChipPtr most_resent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_resent_tab_chip)),
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
              GetActionChipSuggestionsForTab(Eq(page_title), Eq(page_url), _))
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

  ActionChipPtr most_resent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_resent_tab_chip)),
                          Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
}

TEST(ActionChipGeneratorTest,
     StaticChipsAreGeneratedWhenUseOfNewEndpointIsRequestedForDeepDiveChips) {
  EnvironmentFixture env;
  const GURL page_url("https://en.wikipedia.org/wiki/Mathematics");
  const std::u16string page_title(u"Mathematics - Wikipedia");
  TabFixture tab_fixture(page_url, page_title);
  GeneratorFixture generator_fixture;
  generator_fixture.MakeOptimizationGuidePermissive();

  EXPECT_CALL(generator_fixture.mock_service(),
              GetActionChipSuggestionsForTab(Eq(page_title), Eq(page_url), _))
      .Times(0);

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

  ActionChipPtr most_resent_tab_chip =
      CreateStaticRecentTabChip(CreateTabInfo(&tab_fixture.mock_tab()));
  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(most_resent_tab_chip)),
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
  ActionChipPtr expected_recent_tab_chip =
      ActionChip::New(/*title=*/"Ask about previous tab",
                      /*suggestion=*/"Some Title",
                      /*type=*/ChipType::kRecentTab, /*tab=*/tab_info->Clone());

  EXPECT_THAT(actual,
              ElementsAre(Eq(std::cref(expected_recent_tab_chip)),
                          Eq(std::cref(GetStaticDeepSearchChip())),
                          Eq(std::cref(GetStaticImageGenerationChip()))));
}

}  // namespace
