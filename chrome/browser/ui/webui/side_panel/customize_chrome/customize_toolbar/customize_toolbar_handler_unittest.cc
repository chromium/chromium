// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/keyed_service/core/keyed_service.h"
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

  mojo::Receiver<side_panel::customize_chrome::mojom::CustomizeToolbarClient>
      receiver_{this};
};

class MockPinnedToolbarActionsModel : public PinnedToolbarActionsModel {
 public:
  explicit MockPinnedToolbarActionsModel(Profile* profile)
      : PinnedToolbarActionsModel(profile) {}
  MOCK_CONST_METHOD1(Contains, bool(const actions::ActionId& action_id));
  MOCK_METHOD(void,
              UpdatePinnedState,
              (const actions::ActionId& action_id, const bool should_pin));

  MOCK_METHOD(void, AddObserver, (PinnedToolbarActionsModel::Observer*));
  MOCK_METHOD(void, RemoveObserver, (PinnedToolbarActionsModel::Observer*));
};

}  // namespace

class CustomizeToolbarHandlerTest : public BrowserWithTestWindowTest {
 public:
  CustomizeToolbarHandlerTest() = default;

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{PinnedToolbarActionsModelFactory::GetInstance(),
             base::BindRepeating([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
               return std::make_unique<
                   testing::NiceMock<MockPinnedToolbarActionsModel>>(
                   Profile::FromBrowserContext(context));
             })}};
  }

  void SetUp() override {
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
  }

  void TearDown() override {
    pinned_toolbar_actions_model_observer_ = nullptr;
    handler_.reset();
    mock_pinned_toolbar_actions_model_ = nullptr;

    BrowserWithTestWindowTest::TearDown();
  }

  CustomizeToolbarHandler& handler() { return *handler_; }
  MockPinnedToolbarActionsModel& mock_pinned_toolbar_actions_model() {
    return *mock_pinned_toolbar_actions_model_;
  }
  PinnedToolbarActionsModel::Observer* pinned_toolbar_actions_model_observer() {
    return pinned_toolbar_actions_model_observer_;
  }

 protected:
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

  EXPECT_EQ(actions.size(), 8u);

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
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kShowSideSearch));
  // EXPECT_TRUE(
  //     contains_action(side_panel::customize_chrome::mojom::ActionId::kHome));
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kForward));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kNewIncognitoWindow));
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kShowPasswordManager));
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kShowPaymentMethods));
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kShowAddresses));
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kShowDownloads));
  EXPECT_TRUE(contains_action(
      side_panel::customize_chrome::mojom::ActionId::kClearBrowsingData));
  EXPECT_TRUE(
      contains_action(side_panel::customize_chrome::mojom::ActionId::kPrint));
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kShowTranslate));
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kSendTabToSelf));
  // EXPECT_TRUE(contains_action(
  //     side_panel::customize_chrome::mojom::ActionId::kQrCodeGenerator));
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

TEST_F(CustomizeToolbarHandlerTest, ActionAddedRemoved) {
  bool pin;
  side_panel::customize_chrome::mojom::ActionId id;
  EXPECT_CALL(mock_page_, SetActionPinned)
      .Times(2)
      .WillRepeatedly(DoAll(SaveArg<0>(&id), SaveArg<1>(&pin)));

  handler().OnActionAdded(kActionDevTools);
  mock_page_.FlushForTesting();
  EXPECT_EQ(id, side_panel::customize_chrome::mojom::ActionId::kDevTools);
  EXPECT_EQ(pin, true);

  handler().OnActionRemoved(kActionDevTools);
  mock_page_.FlushForTesting();
  EXPECT_EQ(id, side_panel::customize_chrome::mojom::ActionId::kDevTools);
  EXPECT_EQ(pin, false);
}
