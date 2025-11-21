// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_generator.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-data-view.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_mojo_test_utils.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/fake_tab_id_generator.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
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
using ::tabs::TabInterface;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::Return;

int32_t GetTabHandleId(const tabs::TabInterface* tab) {
  return FakeTabIdGenerator::Get()->GenerateTabHandleId(tab);
}

ActionChipsGeneratorImpl CreateActionChipsGenerator() {
  return ActionChipsGeneratorImpl(FakeTabIdGenerator::Get());
}

ActionChip CreateStaticRecentTabChip(TabInfoPtr tab) {
  const std::string title = tab->title;
  return ActionChip(title, "Ask about this tab", ChipType::kRecentTab,
                    std::move(tab));
}

const ActionChip& GetStaticDeepSearchChip() {
  static const base::NoDestructor<ActionChip> kInstance(
      /*title=*/"Research a topic",
      /*suggestion=*/"Dive deep into something new",
      /*type=*/ChipType::kDeepSearch, /*tab=*/nullptr);
  return *kInstance;
}

const ActionChip& GetStaticImageGenerationChip() {
  static const base::NoDestructor<ActionChip> kInstance(
      /*title=*/"Create image",
      /*suggestion=*/"Add an image and reimagine it",
      /*type=*/ChipType::kImage, /*tab=*/nullptr);
  return *kInstance;
}

// A container to store WebContents and its dependency.
// The main usage is to populate TabInterface.
struct TabFixture {
 private:
  // private since there is no reason to make them accessible.
  content::BrowserTaskEnvironment env;
  content::RenderViewHostTestEnabler rvh_test_enabler;

 public:
  std::unique_ptr<TestingProfile> profile;
  std::unique_ptr<content::WebContents> web_contents;
};

std::unique_ptr<TabFixture> CreateTabFixture(const GURL& url,
                                             const std::u16string& title) {
  auto result = std::make_unique<TabFixture>();
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      TemplateURLServiceFactory::GetInstance(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  result->profile = profile_builder.Build();
  result->web_contents = content::WebContentsTester::CreateTestWebContents(
      result->profile.get(), nullptr);
  content::WebContentsTester* tester =
      content::WebContentsTester::For(result->web_contents.get());
  tester->NavigateAndCommit(url);
  tester->SetTitle(title);
  tester->SetLastActiveTime(base::Time::FromMillisecondsSinceUnixEpoch(0));
  return result;
}

using ActionChipGeneratorWithNoRecentTabTest = ::testing::TestWithParam<bool>;

INSTANTIATE_TEST_SUITE_P(ActionChipGeneratorTests,
                         ActionChipGeneratorWithNoRecentTabTest,
                         ::testing::Bool());

TEST_P(ActionChipGeneratorWithNoRecentTabTest,
       GenerateTwoStaticChipsWhenNoTabIsPassed) {
  base::test::SingleThreadTaskEnvironment env;
  base::RunLoop run_loop;
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name,
        GetParam() ? "true" : "false"}});
  std::vector<ActionChipPtr> actual;
  CreateActionChipsGenerator().GenerateActionChips(
      std::nullopt, base::BindLambdaForTesting(
                        [&run_loop, &actual](std::vector<ActionChipPtr> chips) {
                          actual = std::move(chips);
                          run_loop.Quit();
                        }));
  run_loop.Run();
  EXPECT_THAT(
      actual,
      ElementsAre(Pointee(Eq(std::cref(GetStaticDeepSearchChip()))),
                  Pointee(Eq(std::cref(GetStaticImageGenerationChip())))));
}

TEST(ActionChipGeneratorTest,
     GenerateStaticChipsWhenNtpNextShowStaticTextParamIsTrue) {
  const GURL page_url("https://google.com/");
  const std::u16string page_title(u"Google");
  std::unique_ptr<TabFixture> tab_fixture =
      CreateTabFixture(page_url, page_title);
  tabs::MockTabInterface mock_tab;
  EXPECT_CALL(mock_tab, GetContents())
      .WillRepeatedly(Return(tab_fixture->web_contents.get()));

  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      ntp_features::kNtpNextFeatures,
      {{ntp_features::kNtpNextShowStaticTextParam.name, "true"}});

  base::RunLoop run_loop;
  std::vector<ActionChipPtr> actual;
  CreateActionChipsGenerator().GenerateActionChips(
      static_cast<const TabInterface*>(&mock_tab),
      base::BindLambdaForTesting(
          [&run_loop, &actual](std::vector<ActionChipPtr> chips) {
            actual = std::move(chips);
            run_loop.Quit();
          }));
  run_loop.Run();
  ActionChip most_resent_tab_chip = CreateStaticRecentTabChip(
      TabInfo::New(GetTabHandleId(&mock_tab), base::UTF16ToUTF8(page_title),
                   page_url, base::Time::FromMillisecondsSinceUnixEpoch(0)));
  EXPECT_THAT(
      actual,
      ElementsAre(Pointee(Eq(std::cref(most_resent_tab_chip))),
                  Pointee(Eq(std::cref(GetStaticDeepSearchChip()))),
                  Pointee(Eq(std::cref(GetStaticImageGenerationChip())))));
}

}  // namespace
