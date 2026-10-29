// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_ui_controller_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/action_adapter.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/model_adapter.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/platform_adapters_provider.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/adapters/preferences_adapter.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_ui_controller_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

namespace bookmark_bar_api {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;
using PermanentFolderType = bookmarks_api::mojom::PermanentFolderType;

class FakePreferencesAdapter : public PreferencesAdapter {
 public:
  // PreferencesAdapter:
  bool IsAppsShortcutEnabled() const override {
    return options_.apps_shortcut_enabled;
  }
  bool IsTabGroupsEnabled() const override {
    return options_.tab_groups_enabled;
  }
  bool IsManagedBookmarksEnabled() const override {
    return options_.managed_bookmarks_enabled;
  }
  void SetOnChangedCallback(base::RepeatingClosure callback) override {
    on_changed_ = std::move(callback);
  }

  // Sets the options and runs the changed callback.
  void SetOptions(const mojom::DisplayOptions& options) {
    options_ = options;
    if (on_changed_) {
      on_changed_.Run();
    }
  }

  bool has_on_changed_callback() const { return !on_changed_.is_null(); }

 private:
  mojom::DisplayOptions options_;
  base::RepeatingClosure on_changed_;
};

class MockActionAdapter : public ActionAdapter {
 public:
  MOCK_METHOD(void,
              OpenAppsPage,
              (ui::mojom::WindowOpenDisposition),
              (override));
  MOCK_METHOD(void,
              OpenBookmark,
              (const bookmarks::BookmarkNode*,
               ui::mojom::WindowOpenDisposition),
              (override));
  MOCK_METHOD(void, RecordFolderOpened, (), (override));
  MOCK_METHOD(void,
              OpenFolderNodes,
              ((const std::vector<const bookmarks::BookmarkNode*>&),
               ui::mojom::WindowOpenDisposition),
              (override));
  MOCK_METHOD(void,
              ShowContextMenu,
              ((const std::vector<const bookmarks::BookmarkNode*>&),
               const gfx::Point&,
               ui::mojom::MenuSourceType,
               bool,
               base::OnceClosure),
              (override));
};

class MockModelAdapter : public ModelAdapter {
 public:
  MOCK_METHOD(const bookmarks::BookmarkNode*,
              GetNodeById,
              (int64_t),
              (const, override));
  MOCK_METHOD((std::vector<const bookmarks::BookmarkNode*>),
              GetUnderlyingNodes,
              (PermanentFolderType),
              (const, override));
  MOCK_METHOD(void,
              CanPasteFromClipboard,
              ((const std::vector<const bookmarks::BookmarkNode*>&),
               base::OnceCallback<void(bool)>),
              (override));
};

// Points at adapters owned by the test fixture.
class TestInjector : public PlatformAdaptersProvider {
 public:
  TestInjector(ActionAdapter* action,
               PreferencesAdapter* preferences,
               ModelAdapter* model)
      : action_(action), preferences_(preferences), model_(model) {}

  ActionAdapter& action_adapter() override { return *action_; }
  PreferencesAdapter& preferences_adapter() override { return *preferences_; }
  ModelAdapter& model_adapter() override { return *model_; }

 private:
  raw_ptr<ActionAdapter> action_;
  raw_ptr<PreferencesAdapter> preferences_;
  raw_ptr<ModelAdapter> model_;
};

class MockControllerObserver : public BookmarkBarUIControllerObserver {
 public:
  MOCK_METHOD(void,
              OnDisplayOptionsChanged,
              (const mojom::DisplayOptions&),
              (override));
  MOCK_METHOD(void, OnContextMenuClosed, (), (override));
};

class TestRemoteObserver : public mojom::BookmarkBarObserver {
 public:
  explicit TestRemoteObserver(
      mojo::PendingAssociatedReceiver<mojom::BookmarkBarObserver> receiver)
      : receiver_(this, std::move(receiver)) {}

  // Waits for the next OnDisplayOptionsChanged() and returns its options.
  mojom::DisplayOptionsPtr WaitForDisplayOptions() {
    return display_options_.Take();
  }

  // mojom::BookmarkBarObserver:
  void OnDisplayOptionsChanged(mojom::DisplayOptionsPtr options) override {
    display_options_.SetValue(std::move(options));
  }
  void OnContextMenuClosed() override {}

 private:
  mojo::AssociatedReceiver<mojom::BookmarkBarObserver> receiver_;
  base::test::TestFuture<mojom::DisplayOptionsPtr> display_options_;
};

mojom::NodeIdPtr NodeId(int64_t id) {
  return mojom::NodeId::NewNodeId(id);
}

class BookmarkBarUIControllerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    controller_ = std::make_unique<BookmarkBarUIControllerImpl>(
        std::make_unique<TestInjector>(&action_adapter_, &preferences_adapter_,
                                       &model_adapter_));
  }

  void TearDown() override { controller_.reset(); }

  base::test::TaskEnvironment task_environment_;
  FakePreferencesAdapter preferences_adapter_;
  testing::NiceMock<MockActionAdapter> action_adapter_;
  testing::NiceMock<MockModelAdapter> model_adapter_;
  std::unique_ptr<BookmarkBarUIControllerImpl> controller_;
};

TEST_F(BookmarkBarUIControllerImplTest, DisplayOptionsChangesNotifyObservers) {
  testing::StrictMock<MockControllerObserver> observer;
  controller_->AddObserver(&observer);

  const mojom::DisplayOptions expected(/*apps_shortcut_enabled=*/true,
                                       /*tab_groups_enabled=*/false,
                                       /*managed_bookmarks_enabled=*/true);
  EXPECT_CALL(observer, OnDisplayOptionsChanged(testing::Truly(
                            [&expected](const mojom::DisplayOptions& options) {
                              return options.Equals(expected);
                            })));
  preferences_adapter_.SetOptions(expected);

  controller_->RemoveObserver(&observer);
}

TEST_F(BookmarkBarUIControllerImplTest, DestructionClearsOnChangedCallback) {
  EXPECT_TRUE(preferences_adapter_.has_on_changed_callback());
  controller_.reset();
  EXPECT_FALSE(preferences_adapter_.has_on_changed_callback());
}

// Gets the current options over Mojo, then receives an update on the stream.
TEST_F(BookmarkBarUIControllerImplTest, GetDisplayOptionsOverMojo) {
  const mojom::DisplayOptions initial(/*apps_shortcut_enabled=*/true,
                                      /*tab_groups_enabled=*/false,
                                      /*managed_bookmarks_enabled=*/false);
  preferences_adapter_.SetOptions(initial);
  mojo::Remote<mojom::BookmarkBarUIController> remote;
  controller_->Accept(remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<
      mojom::BookmarkBarUIController::GetDisplayOptionsResult>
      future;
  remote->GetDisplayOptions(future.GetCallback());
  auto result = future.Take();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value()->options->Equals(initial));
  TestRemoteObserver observer(std::move(result.value()->stream));

  const mojom::DisplayOptions updated(/*apps_shortcut_enabled=*/false,
                                      /*tab_groups_enabled=*/true,
                                      /*managed_bookmarks_enabled=*/false);
  preferences_adapter_.SetOptions(updated);
  EXPECT_TRUE(observer.WaitForDisplayOptions()->Equals(updated));
}

TEST_F(BookmarkBarUIControllerImplTest, OpenBookmarkDelegates) {
  auto bookmark_model = bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0, u"Title",
                             GURL("https://google.com"));

  EXPECT_CALL(model_adapter_, GetNodeById(42)).WillOnce(Return(node));
  EXPECT_CALL(action_adapter_,
              OpenBookmark(node, ui::mojom::WindowOpenDisposition::kNewWindow));
  EXPECT_TRUE(
      controller_
          ->OpenBookmark(42, ui::mojom::WindowOpenDisposition::kNewWindow)
          .has_value());
}

TEST_F(BookmarkBarUIControllerImplTest, OpenBookmarkNodeNotFound) {
  EXPECT_CALL(model_adapter_, GetNodeById(42)).WillOnce(Return(nullptr));
  EXPECT_CALL(action_adapter_, OpenBookmark(_, _)).Times(0);

  auto result = controller_->OpenBookmark(
      42, ui::mojom::WindowOpenDisposition::kNewWindow);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
}

TEST_F(BookmarkBarUIControllerImplTest, OpenAllInFolderOpensPermanentFolder) {
  auto bookmark_model = bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* bar_node = bookmark_model->bookmark_bar_node();

  EXPECT_CALL(model_adapter_,
              GetUnderlyingNodes(PermanentFolderType::kBookmarkBar))
      .WillOnce(Return(std::vector<const bookmarks::BookmarkNode*>{bar_node}));
  EXPECT_CALL(action_adapter_,
              OpenFolderNodes(ElementsAre(bar_node),
                              ui::mojom::WindowOpenDisposition::kNewWindow));

  EXPECT_TRUE(
      controller_
          ->OpenAllInFolder(mojom::NodeId::NewPermanentFolder(
                                PermanentFolderType::kBookmarkBar),
                            ui::mojom::WindowOpenDisposition::kNewWindow)
          .has_value());
}

TEST_F(BookmarkBarUIControllerImplTest, OpenAllInFolderOpensFolderNode) {
  auto bookmark_model = bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* folder = bookmark_model->AddFolder(
      bookmark_model->bookmark_bar_node(), 0, u"Folder");

  EXPECT_CALL(model_adapter_, GetNodeById(folder->id()))
      .WillOnce(Return(folder));
  EXPECT_CALL(model_adapter_, GetUnderlyingNodes(_)).Times(0);
  EXPECT_CALL(action_adapter_,
              OpenFolderNodes(ElementsAre(folder),
                              ui::mojom::WindowOpenDisposition::kNewWindow));

  EXPECT_TRUE(
      controller_
          ->OpenAllInFolder(NodeId(folder->id()),
                            ui::mojom::WindowOpenDisposition::kNewWindow)
          .has_value());
}

// Shows the menu once the paste check finishes, and notifies observers when it
// closes.
TEST_F(BookmarkBarUIControllerImplTest, ShowContextMenuDelegates) {
  auto bookmark_model = bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0, u"Title",
                             GURL("https://google.com"));
  const gfx::Point point(100, 200);
  const ui::mojom::MenuSourceType source_type =
      ui::mojom::MenuSourceType::kMouse;

  EXPECT_CALL(model_adapter_, GetNodeById(node->id()))
      .Times(2)
      .WillRepeatedly(Return(node));
  EXPECT_CALL(model_adapter_, CanPasteFromClipboard(ElementsAre(node), _))
      .WillOnce(base::test::RunOnceCallback<1>(true));
  base::OnceClosure on_close;
  EXPECT_CALL(action_adapter_,
              ShowContextMenu(ElementsAre(node), point, source_type, true, _))
      .WillOnce(MoveArg<4>(&on_close));

  testing::StrictMock<MockControllerObserver> observer;
  controller_->AddObserver(&observer);
  EXPECT_TRUE(
      controller_->ShowContextMenu(NodeId(node->id()), point, source_type)
          .has_value());

  ASSERT_TRUE(on_close);
  EXPECT_CALL(observer, OnContextMenuClosed());
  std::move(on_close).Run();
  controller_->RemoveObserver(&observer);
}

// An unknown ID, the root, a permanent folder named by node ID, and kUnknown
// are all rejected before the paste check.
TEST_F(BookmarkBarUIControllerImplTest, ShowContextMenuRejectsInvalidIds) {
  auto bookmark_model = bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* root = bookmark_model->root_node();
  const bookmarks::BookmarkNode* bar_node = bookmark_model->bookmark_bar_node();
  ON_CALL(model_adapter_, GetNodeById(_)).WillByDefault(Return(nullptr));
  ON_CALL(model_adapter_, GetNodeById(root->id())).WillByDefault(Return(root));
  ON_CALL(model_adapter_, GetNodeById(bar_node->id()))
      .WillByDefault(Return(bar_node));
  EXPECT_CALL(model_adapter_, GetUnderlyingNodes(_)).Times(0);
  EXPECT_CALL(model_adapter_, CanPasteFromClipboard(_, _)).Times(0);

  mojom::NodeIdPtr invalid_ids[] = {
      NodeId(42), NodeId(root->id()), NodeId(bar_node->id()),
      mojom::NodeId::NewPermanentFolder(PermanentFolderType::kUnknown)};
  for (mojom::NodeIdPtr& id : invalid_ids) {
    auto result = controller_->ShowContextMenu(
        std::move(id), gfx::Point(), ui::mojom::MenuSourceType::kMouse);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error()->code, mojo_base::mojom::Code::kNotFound);
  }
}

TEST_F(BookmarkBarUIControllerImplTest,
       ShowContextMenuNodeRemovedDuringPasteCheckNotifiesClosed) {
  auto bookmark_model = bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddURL(bookmark_model->bookmark_bar_node(), 0, u"Title",
                             GURL("https://google.com"));

  // Found for the initial check, gone by the time the paste check finishes.
  EXPECT_CALL(model_adapter_, GetNodeById(node->id()))
      .WillOnce(Return(node))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(model_adapter_, CanPasteFromClipboard(_, _))
      .WillOnce(base::test::RunOnceCallback<1>(true));
  EXPECT_CALL(action_adapter_, ShowContextMenu(_, _, _, _, _)).Times(0);

  testing::StrictMock<MockControllerObserver> observer;
  controller_->AddObserver(&observer);
  EXPECT_CALL(observer, OnContextMenuClosed());
  EXPECT_TRUE(controller_
                  ->ShowContextMenu(NodeId(node->id()), gfx::Point(),
                                    ui::mojom::MenuSourceType::kMouse)
                  .has_value());
  controller_->RemoveObserver(&observer);
}

}  // namespace

}  // namespace bookmark_bar_api
