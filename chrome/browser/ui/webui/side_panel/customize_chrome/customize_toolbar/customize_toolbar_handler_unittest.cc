// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar_handler.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model_factory.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar.mojom.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"

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
  MOCK_METHOD(bool, Contains, (actions::ActionId action_id), (const));
  MOCK_METHOD(const std::vector<actions::ActionId>&,
              PinnedActionIds,
              (),
              (const));
  MOCK_METHOD(void,
              UpdatePinnedState,
              (actions::ActionId action_id, const bool should_pin));
  MOCK_METHOD(void, ResetToDefault, ());

  MOCK_METHOD(void, AddObserver, (PinnedToolbarActionsModel::Observer*));
  MOCK_METHOD(void, RemoveObserver, (PinnedToolbarActionsModel::Observer*));
};

}  // namespace

class CustomizeToolbarHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  CustomizeToolbarHandlerTest() = default;

  TestingProfile::TestingFactories GetTestingFactories() const override {
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
    InitializeActionIdStringMapping();
    ChromeRenderViewHostTestHarness::SetUp();

    ON_CALL(mock_browser_window_, GetProfile())
        .WillByDefault(testing::Return(profile()));

    webui::SetBrowserWindowInterface(web_contents(), &mock_browser_window_);

    mock_pinned_toolbar_actions_model_ =
        static_cast<MockPinnedToolbarActionsModel*>(
            PinnedToolbarActionsModelFactory::GetForProfile(profile()));

    EXPECT_CALL(mock_pinned_toolbar_actions_model(), AddObserver)
        .Times(testing::AtLeast(1))
        .WillOnce(SaveArg<0>(&pinned_toolbar_actions_model_observer_))
        .WillRepeatedly(testing::Return());

    handler_ = std::make_unique<CustomizeToolbarHandler>(
        mojo::PendingReceiver<
            side_panel::customize_chrome::mojom::CustomizeToolbarHandler>(),
        mock_page_.BindAndGetRemote(), web_contents());
    mock_page_.FlushForTesting();
    EXPECT_EQ(handler_.get(), pinned_toolbar_actions_model_observer_);

    task_environment()->RunUntilIdle();

    auto* const template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  }

  void TearDown() override {
    webui::SetBrowserWindowInterface(web_contents(), nullptr);
    pinned_toolbar_actions_model_observer_ = nullptr;
    handler_.reset();
    mock_pinned_toolbar_actions_model_ = nullptr;
    actions::ActionIdMap::ResetMapsForTesting();
    testing::Mock::VerifyAndClear(&mock_browser_window_);

    ChromeRenderViewHostTestHarness::TearDown();
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
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_;

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

TEST_F(CustomizeToolbarHandlerTest, PinSplitTab) {
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kPinSplitTabButton));

  handler().PinAction(side_panel::customize_chrome::mojom::ActionId::kSplitTab,
                      false);
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kPinSplitTabButton));

  handler().PinAction(side_panel::customize_chrome::mojom::ActionId::kSplitTab,
                      true);
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kPinSplitTabButton));
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

TEST_F(CustomizeToolbarHandlerTest, SplitTabPrefUpdated) {
  bool pin;
  side_panel::customize_chrome::mojom::ActionId id;
  EXPECT_CALL(mock_page_, SetActionPinned)
      .Times(2)
      .WillRepeatedly(DoAll(SaveArg<0>(&id), SaveArg<1>(&pin)));

  profile()->GetPrefs()->SetBoolean(prefs::kPinSplitTabButton, false);
  mock_page_.FlushForTesting();
  EXPECT_EQ(id, side_panel::customize_chrome::mojom::ActionId::kSplitTab);
  EXPECT_EQ(pin, false);

  profile()->GetPrefs()->SetBoolean(prefs::kPinSplitTabButton, true);
  mock_page_.FlushForTesting();
  EXPECT_EQ(id, side_panel::customize_chrome::mojom::ActionId::kSplitTab);
  EXPECT_EQ(pin, true);
}


TEST_F(CustomizeToolbarHandlerTest, ResetToDefault) {
  EXPECT_CALL(mock_pinned_toolbar_actions_model(), ResetToDefault).Times(1);
  handler().ResetToDefault();
}

TEST_F(CustomizeToolbarHandlerTest, GetIsCustomizedForNonPinnedActions) {
  EXPECT_CALL(mock_pinned_toolbar_actions_model(), ResetToDefault())
      .WillRepeatedly([this]() {
        mock_pinned_toolbar_actions_model()
            .PinnedToolbarActionsModel::ResetToDefault();
      });

  // Initially, everything is default.
  // Home: False, Forward: True, SplitTab: False
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowForwardButton));
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kPinSplitTabButton));

  {
    base::MockCallback<CustomizeToolbarHandler::GetIsCustomizedCallback>
        callback;
    EXPECT_CALL(callback, Run(false)).Times(1);
    handler().GetIsCustomized(callback.Get());
  }

  // Toggle all to non-default values.
  handler().PinAction(side_panel::customize_chrome::mojom::ActionId::kHome,
                      true);
  handler().PinAction(side_panel::customize_chrome::mojom::ActionId::kForward,
                      false);
  handler().PinAction(side_panel::customize_chrome::mojom::ActionId::kSplitTab,
                      true);

  // Verify all prefs changed.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowForwardButton));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kPinSplitTabButton));

  // Verify GetIsCustomized returns true.
  {
    base::MockCallback<CustomizeToolbarHandler::GetIsCustomizedCallback>
        callback;
    EXPECT_CALL(callback, Run(true)).Times(1);
    handler().GetIsCustomized(callback.Get());
  }

  // Reset to default.
  handler().ResetToDefault();

  // Verify all reset to default.
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kShowForwardButton));
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kPinSplitTabButton));

  // Verify GetIsCustomized returns false.
  {
    base::MockCallback<CustomizeToolbarHandler::GetIsCustomizedCallback>
        callback;
    EXPECT_CALL(callback, Run(false)).Times(1);
    handler().GetIsCustomized(callback.Get());
  }
}



TEST_F(CustomizeToolbarHandlerTest, ChangeBrowserWhileOpen) {
  // Create a second web contents and mock browser interface.
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_2;
  ON_CALL(mock_browser_2, GetProfile())
      .WillByDefault(testing::Return(profile()));

  std::unique_ptr<content::WebContents> web_contents_2 =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
  webui::SetBrowserWindowInterface(web_contents_2.get(), &mock_browser_2);

  // Set up a second handler associated with that tab in the second browser
  // interface.
  testing::NiceMock<MockPage> mock_page_2;
  std::unique_ptr<CustomizeToolbarHandler> handler_2 =
      std::make_unique<CustomizeToolbarHandler>(
          mojo::PendingReceiver<
              side_panel::customize_chrome::mojom::CustomizeToolbarHandler>(),
          mock_page_2.BindAndGetRemote(), web_contents_2.get());
  task_environment()->RunUntilIdle();

  // Set BrowserWindowInterface to nullptr (e.g. when tab is detaching/moving).
  webui::SetBrowserWindowInterface(web_contents_2.get(), nullptr);

  // Verify PinAction continues to work without crashing.
  EXPECT_CALL(mock_pinned_toolbar_actions_model(),
              UpdatePinnedState(kActionSidePanelShowBookmarks, true))
      .Times(1);
  handler_2->PinAction(
      side_panel::customize_chrome::mojom::ActionId::kShowBookmarks, true);

  // Verify ListActions gracefully returns an empty list when
  // BrowserWindowInterface is null.
  std::vector<side_panel::customize_chrome::mojom::ActionPtr> actions;
  base::MockCallback<CustomizeToolbarHandler::ListActionsCallback> callback;
  EXPECT_CALL(callback, Run(_)).WillOnce(MoveArg(&actions));
  handler_2->ListActions(callback.Get());
  EXPECT_TRUE(actions.empty());
}
