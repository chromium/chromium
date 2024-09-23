// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/lens/lens_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace {

using testing::_;
using testing::DoAll;
using testing::SaveArg;

class MockPage
    : public side_panel::customize_chrome::mojom::CustomizeToolbarClient {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<
      side_panel::customize_chrome::mojom::CustomizeToolbarClient>
  BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              SetActionPinned,
              (side_panel::customize_chrome::mojom::ActionId action_id,
               bool pinned));
  MOCK_METHOD(void, NotifyActionsUpdated, ());

  mojo::Receiver<side_panel::customize_chrome::mojom::CustomizeToolbarClient>
      receiver_{this};
};

class MockPinnedToolbarActionsModel : public PinnedToolbarActionsModel {
 public:
  explicit MockPinnedToolbarActionsModel(Profile* profile)
      : PinnedToolbarActionsModel(profile) {}
  MOCK_METHOD(bool, Contains, (const actions::ActionId& action_id), (const));
  MOCK_METHOD(const std::vector<actions::ActionId>&,
              PinnedActionIds,
              (),
              (const));
  MOCK_METHOD(void,
              UpdatePinnedState,
              (const actions::ActionId& action_id, const bool should_pin));
  MOCK_METHOD(void, ResetToDefault, ());

  MOCK_METHOD(void, AddObserver, (PinnedToolbarActionsModel::Observer*));
  MOCK_METHOD(void, RemoveObserver, (PinnedToolbarActionsModel::Observer*));
};

}  // namespace

class CustomizeToolbarHandlerTest : public BrowserWithTestWindowTest {
 public:
  CustomizeToolbarHandlerTest() = default;

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {
        TestingProfile::TestingFactory{
            PinnedToolbarActionsModelFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  testing::NiceMock<MockPinnedToolbarActionsModel>>(
                  Profile::FromBrowserContext(context));
            })},
        TestingProfile::TestingFactory{
            TemplateURLServiceFactory::GetInstance(),
            base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)}};
  }

  void SetUp() override {
    SetupFeatureList();
    InitializeActionIdStringMapping();
    BrowserWithTestWindowTest::SetUp();

    mock_pinned_toolbar_actions_model_ =
        static_cast<MockPinnedToolbarActionsModel*>(
            PinnedToolbarActionsModelFactory::GetForProfile(profile()));

    EXPECT_CALL(mock_pinned_toolbar_actions_model(), AddObserver)
        .Times(1)
        .WillOnce(SaveArg<0>(&pinned_toolbar_actions_model_observer_));

    handler_ = std::make_unique<CustomizeToolbarHandler>(
        mojo::PendingReceiver<
            side_panel::customize_chrome::mojom::CustomizeToolbarHandler>(),
        mock_page_.BindAndGetRemote(), browser());
    mock_page_.FlushForTesting();
    EXPECT_EQ(handler_.get(), pinned_toolbar_actions_model_observer_);

    task_environment()->RunUntilIdle();

    auto* const template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  }

  void TearDown() override {
    pinned_toolbar_actions_model_observer_ = nullptr;
    handler_.reset();
    mock_pinned_toolbar_actions_model_ = nullptr;

    BrowserWithTestWindowTest::TearDown();
  }

  virtual void SetupFeatureList() {
    feature_list_.InitWithFeatures(
        {features::kToolbarPinning, lens::features::kLensOverlay}, {});
  }

  CustomizeToolbarHandler& handler() { return *handler_; }
  MockPinnedToolbarActionsModel& mock_pinned_toolbar_actions_model() {
    return *mock_pinned_toolbar_actions_model_;
  }
  PinnedToolbarActionsModel::Observer* pinned_toolbar_actions_model_observer() {
    return pinned_toolbar_actions_model_observer_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  testing::NiceMock<MockPage> mock_page_;

  raw_ptr<MockPinnedToolbarActionsModel> mock_pinned_toolbar_actions_model_;
  raw_ptr<PinnedToolbarActionsModel::Observer>
      pinned_toolbar_actions_model_observer_;
  std::unique_ptr<CustomizeToolbarHandler> handler_;
};

TEST_F(CustomizeToolbarHandlerTest, ListCategories) {
  std::vector<side_panel::customize_chrome::mojom::CategoryPtr> categories;
  base::MockCallback<CustomizeToolbarHandler::ListCategoriesCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(MoveArg(&categories));
  handler().ListCategories(callback.Get());

  const int min_category_id = static_cast<int>(
      side_panel::customize_chrome::mojom::CategoryId::kMinValue);
  const int max_category_id = static_cast<int>(
      side_panel::customize_chrome::mojom::CategoryId::kMaxValue);

  EXPECT_EQ(static_cast<int>(categories.size()), max_category_id + 1);
  for (int id = min_category_id; id < max_category_id; id++) {
    EXPECT_NE(std::find_if(
                  categories.begin(), categories.end(),
                  [id](side_panel::customize_chrome::mojom::CategoryPtr& cat) {
                    return static_cast<int>(cat->id) == id;
                  }),
              categories.end());
  }
}

TEST_F(CustomizeToolbarHandlerTest, ListActions) {
  std::vector<side_panel::customize_chrome::mojom::ActionPtr> actions;
  base::MockCallback<CustomizeToolbarHandler::ListActionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(MoveArg(&actions));
  handler().ListActions(callback.Get());

  const auto contains_action =
      [&actions](side_panel::customize_chrome::mojom::ActionId id) -> bool {
    return std::find_if(
               actions.begin(), actions.end(),
               [id](side_panel::customize_chrome::mojom::ActionPtr& action) {
                 return action->id == id;
               }) != actions.end();
  };

  // 11 actions are currently pinnable; more should be pinnable in the future.
  EXPECT_GE(actions.size(), 11u);

  // History clusters aren't enabled for this testing profile. The rest of the
  // commented out ones aren't pinnable yet.
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowBookmarks));
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kShowHistoryCluster));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowReadAnything));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowReadingList));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowLensOverlay));
  EXPECT_TRUE(
      contains_action(side_panel::customize_chrome::mojom::ActionId::kHome));
  EXPECT_TRUE(
      contains_action(side_panel::customize_chrome::mojom::ActionId::kForward));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kNewIncognitoWindow));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowPasswordManager));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowPaymentMethods));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowAddresses));
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kShowDownloads));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kClearBrowsingData));
  EXPECT_TRUE(
      contains_action(side_panel::customize_chrome::mojom::ActionId::kPrint));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowTranslate));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kSendTabToSelf));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kQrCodeGenerator));
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kRouteMedia));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kTaskManager));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kDevTools));
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kShowChromeLabs));
}

TEST_F(CustomizeToolbarHandlerTest, PinAction) {
  bool pin;
  actions::ActionId id;
  EXPECT_CALL(mock_pinned_toolbar_actions_model(), UpdatePinnedState)
      .Times(2)
      .WillRepeatedly(DoAll(SaveArg<0>(&id), SaveArg<1>(&pin)));

  handler().PinAction(side_panel::customize_chrome::mojom::ActionId::kDevTools,
                      true);
  EXPECT_EQ(id, kActionDevTools);
  EXPECT_EQ(pin, true);

  handler().PinAction(side_panel::customize_chrome::mojom::ActionId::kDevTools,
                      false);
  EXPECT_EQ(id, kActionDevTools);
  EXPECT_EQ(pin, false);
}

TEST_F(CustomizeToolbarHandlerTest, PinHome) {
  ASSERT_EQ(false, profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));

  handler().PinAction(side_panel::customize_chrome::mojom::ActionId::kHome,
                      true);
  EXPECT_EQ(true, profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));

  handler().PinAction(side_panel::customize_chrome::mojom::ActionId::kHome,
                      false);
  EXPECT_EQ(false, profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
}

TEST_F(CustomizeToolbarHandlerTest, PinForward) {
  ASSERT_EQ(true, profile()->GetPrefs()->GetBoolean(prefs::kShowForwardButton));

  handler().PinAction(side_panel::customize_chrome::mojom::ActionId::kForward,
                      false);
  EXPECT_EQ(false,
            profile()->GetPrefs()->GetBoolean(prefs::kShowForwardButton));

  handler().PinAction(side_panel::customize_chrome::mojom::ActionId::kForward,
                      true);
  EXPECT_EQ(true, profile()->GetPrefs()->GetBoolean(prefs::kShowForwardButton));
}

TEST_F(CustomizeToolbarHandlerTest, ActionsChanged) {
  EXPECT_CALL(mock_page_, NotifyActionsUpdated).Times(1);

  handler().OnActionsChanged();
  mock_page_.FlushForTesting();
}

TEST_F(CustomizeToolbarHandlerTest, HomePrefUpdated) {
  bool pin;
  side_panel::customize_chrome::mojom::ActionId id;
  EXPECT_CALL(mock_page_, SetActionPinned)
      .Times(2)
      .WillRepeatedly(DoAll(SaveArg<0>(&id), SaveArg<1>(&pin)));

  profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, true);
  mock_page_.FlushForTesting();
  EXPECT_EQ(id, side_panel::customize_chrome::mojom::ActionId::kHome);
  EXPECT_EQ(pin, true);

  profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, false);
  mock_page_.FlushForTesting();
  EXPECT_EQ(id, side_panel::customize_chrome::mojom::ActionId::kHome);
  EXPECT_EQ(pin, false);
}

TEST_F(CustomizeToolbarHandlerTest, ForwardPrefUpdated) {
  bool pin;
  side_panel::customize_chrome::mojom::ActionId id;
  EXPECT_CALL(mock_page_, SetActionPinned)
      .Times(2)
      .WillRepeatedly(DoAll(SaveArg<0>(&id), SaveArg<1>(&pin)));

  profile()->GetPrefs()->SetBoolean(prefs::kShowForwardButton, false);
  mock_page_.FlushForTesting();
  EXPECT_EQ(id, side_panel::customize_chrome::mojom::ActionId::kForward);
  EXPECT_EQ(pin, false);

  profile()->GetPrefs()->SetBoolean(prefs::kShowForwardButton, true);
  mock_page_.FlushForTesting();
  EXPECT_EQ(id, side_panel::customize_chrome::mojom::ActionId::kForward);
  EXPECT_EQ(pin, true);
}

TEST_F(CustomizeToolbarHandlerTest, ResetToDefault) {
  EXPECT_CALL(mock_pinned_toolbar_actions_model(), ResetToDefault).Times(1);
  handler().ResetToDefault();
}

TEST_F(CustomizeToolbarHandlerTest, ActionsUpdatedOnVisibilityChange) {
  std::vector<side_panel::customize_chrome::mojom::ActionPtr> actions;
  base::MockCallback<CustomizeToolbarHandler::ListActionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(MoveArg(&actions));
  handler().ListActions(callback.Get());

  const auto contains_action =
      [&actions](side_panel::customize_chrome::mojom::ActionId id) -> bool {
    return std::find_if(
               actions.begin(), actions.end(),
               [id](side_panel::customize_chrome::mojom::ActionPtr& action) {
                 return action->id == id;
               }) != actions.end();
  };

  // Devtools is initially present in the actions list.
  ASSERT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kDevTools));

  // Set visibility of devtools to false, and...
  actions::ActionItem* const scope_action =
      browser()->browser_actions()->root_action_item();
  actions::ActionItem* const devtools_action_item =
      actions::ActionManager::Get().FindAction(kActionDevTools, scope_action);

  // The WebUI client is notified, and...
  EXPECT_CALL(mock_page_, NotifyActionsUpdated).Times(1);
  devtools_action_item->SetVisible(false);

  // Devtools is now absent from the actions list.
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(MoveArg(&actions));
  handler().ListActions(callback.Get());
  EXPECT_FALSE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kDevTools));
}

class CustomizeToolbarHandlerCompanionTest
    : public CustomizeToolbarHandlerTest {
 public:
  CustomizeToolbarHandlerCompanionTest() = default;

 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeatures(
        {companion::features::internal::kSidePanelCompanion},
        {lens::features::kLensOverlay});
  }
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
TEST_F(CustomizeToolbarHandlerCompanionTest, ListActionsContainsCompanion) {
  std::vector<side_panel::customize_chrome::mojom::ActionPtr> actions;
  base::MockCallback<CustomizeToolbarHandler::ListActionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(MoveArg(&actions));
  handler().ListActions(callback.Get());

  const auto contains_action =
      [&actions](side_panel::customize_chrome::mojom::ActionId id) -> bool {
    return std::find_if(
               actions.begin(), actions.end(),
               [id](side_panel::customize_chrome::mojom::ActionPtr& action) {
                 return action->id == id;
               }) != actions.end();
  };

  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kShowSearchCompanion));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
