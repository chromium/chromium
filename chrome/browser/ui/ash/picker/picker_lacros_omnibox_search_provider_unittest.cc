// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_lacros_omnibox_search_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/crosapi/search_controller_ash.h"
#include "chrome/browser/ash/crosapi/search_controller_factory_ash.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom-forward.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TODO: b/326147929 - Share this code with `crosapi::SearchControllerAsh` and
// `crosapi::SearchControllerFactoryAsh` unit tests.
class TestMojomSearchController : public crosapi::mojom::SearchController {
 public:
  explicit TestMojomSearchController(bool bookmarks,
                                     bool history,
                                     bool open_tabs)
      : bookmarks_(bookmarks), history_(history), open_tabs_(open_tabs) {}
  ~TestMojomSearchController() override = default;

  void Bind(mojo::PendingReceiver<crosapi::mojom::SearchController> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void RunUntilSearch() {
    base::RunLoop loop;
    base::AutoReset<base::RepeatingClosure> quit_loop(&search_callback_,
                                                      loop.QuitClosure());
    loop.Run();
  }

  const std::u16string& last_query() { return last_query_; }
  bool bookmarks() const { return bookmarks_; }
  bool history() const { return history_; }
  bool open_tabs() const { return open_tabs_; }

 private:
  void Search(const std::u16string& query, SearchCallback callback) override {
    last_query_ = query;
    // We are not interested in the search callback in the class under test, but
    // we still need to run the callback with a valid receiver.
    std::move(callback).Run(
        mojo::AssociatedRemote<crosapi::mojom::SearchResultsPublisher>()
            .BindNewEndpointAndPassReceiver());

    search_callback_.Run();
  }

  base::RepeatingClosure search_callback_ = base::DoNothing();

  mojo::Receiver<crosapi::mojom::SearchController> receiver_{this};
  bool bookmarks_;
  bool history_;
  bool open_tabs_;
  std::u16string last_query_;
};

class TestMojomSearchControllerFactory
    : public crosapi::mojom::SearchControllerFactory {
 public:
  TestMojomSearchControllerFactory() = default;
  ~TestMojomSearchControllerFactory() override = default;

  mojo::PendingRemote<crosapi::mojom::SearchControllerFactory> BindToRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void RunUntilCreateSearchController() {
    base::RunLoop loop;
    base::AutoReset<base::RepeatingClosure> quit_loop(
        &create_search_controller_callback_, loop.QuitClosure());
    loop.Run();
  }

  std::unique_ptr<TestMojomSearchController> TakeLastTestController() {
    return std::move(last_test_controller_);
  }

  // cam::SearchControllerFactory overrides:
  void CreateSearchControllerPicker(
      mojo::PendingReceiver<crosapi::mojom::SearchController> controller,
      bool bookmark,
      bool history,
      bool open_tab) override {
    CHECK(!last_test_controller_);
    last_test_controller_ = std::make_unique<TestMojomSearchController>(
        bookmark, history, open_tab);
    last_test_controller_->Bind(std::move(controller));

    create_search_controller_callback_.Run();
  }

 private:
  base::RepeatingClosure create_search_controller_callback_ = base::DoNothing();

  std::unique_ptr<TestMojomSearchController> last_test_controller_;
  mojo::Receiver<crosapi::mojom::SearchControllerFactory> receiver_{this};
};

using PickerLacrosOmniboxSearchProviderTest = ::testing::Test;

TEST_F(PickerLacrosOmniboxSearchProviderTest,
       ControllerIsNullptrWhenNotBoundOnConstruction) {
  base::test::SingleThreadTaskEnvironment environment;
  crosapi::SearchControllerFactoryAsh factory;
  ASSERT_FALSE(factory.IsBound());

  PickerLacrosOmniboxSearchProvider provider(&factory, false, false, false);
  crosapi::SearchControllerAsh* controller = provider.GetController();

  EXPECT_FALSE(controller);
}

TEST_F(PickerLacrosOmniboxSearchProviderTest,
       ControllerIsNonNullWhenBoundOnConstruction) {
  base::test::SingleThreadTaskEnvironment environment;
  crosapi::SearchControllerFactoryAsh factory;
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());
  ASSERT_TRUE(factory.IsBound());

  PickerLacrosOmniboxSearchProvider provider(&factory, false, false, false);
  crosapi::SearchControllerAsh* controller = provider.GetController();

  EXPECT_TRUE(controller);
}

TEST_F(PickerLacrosOmniboxSearchProviderTest,
       ControllerIsNonNullWhenBoundAfterConstruction) {
  base::test::SingleThreadTaskEnvironment environment;
  crosapi::SearchControllerFactoryAsh factory;
  ASSERT_FALSE(factory.IsBound());

  PickerLacrosOmniboxSearchProvider provider(&factory, false, false, false);
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());
  ASSERT_TRUE(factory.IsBound());
  crosapi::SearchControllerAsh* controller = provider.GetController();

  EXPECT_TRUE(controller);
}

TEST_F(PickerLacrosOmniboxSearchProviderTest,
       CallsFactoryOnConstructionIfBound) {
  base::test::SingleThreadTaskEnvironment environment;
  crosapi::SearchControllerFactoryAsh factory;
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());
  ASSERT_TRUE(factory.IsBound());

  PickerLacrosOmniboxSearchProvider provider(&factory, false, false, false);
  mojom_factory.RunUntilCreateSearchController();

  EXPECT_TRUE(mojom_factory.TakeLastTestController());
}

TEST_F(PickerLacrosOmniboxSearchProviderTest,
       CallsFactoryAfterConstructionWhenBound) {
  base::test::SingleThreadTaskEnvironment environment;
  crosapi::SearchControllerFactoryAsh factory;
  ASSERT_FALSE(factory.IsBound());

  PickerLacrosOmniboxSearchProvider provider(&factory, false, false, false);
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());
  ASSERT_TRUE(factory.IsBound());
  mojom_factory.RunUntilCreateSearchController();

  EXPECT_TRUE(mojom_factory.TakeLastTestController());
}

TEST_F(PickerLacrosOmniboxSearchProviderTest, CallsFactoryWhenRebound) {
  base::test::SingleThreadTaskEnvironment environment;
  crosapi::SearchControllerFactoryAsh factory;
  PickerLacrosOmniboxSearchProvider provider(&factory, false, false, false);
  {
    // Connect a remote factory...
    TestMojomSearchControllerFactory mojom_factory;
    factory.BindRemote(mojom_factory.BindToRemote());
    mojom_factory.RunUntilCreateSearchController();
    ASSERT_TRUE(mojom_factory.TakeLastTestController());
    // ...then disconnect it.
  }
  // Ensure that the factory receives the disconnection so it can be rebound.
  // TODO: b/326147929 - Use a `QuitClosure` for this.
  base::RunLoop().RunUntilIdle();

  // Rebind another remote factory.
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());
  mojom_factory.RunUntilCreateSearchController();

  EXPECT_TRUE(mojom_factory.TakeLastTestController());
}

TEST_F(PickerLacrosOmniboxSearchProviderTest, DoesNotCallFactoryMultipleTimes) {
  base::test::SingleThreadTaskEnvironment environment;
  crosapi::SearchControllerFactoryAsh factory;
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());

  PickerLacrosOmniboxSearchProvider provider(&factory, false, false, false);
  mojom_factory.RunUntilCreateSearchController();
  ASSERT_TRUE(mojom_factory.TakeLastTestController());
  (void)provider.GetController();
  (void)provider.GetController();
  // Ensure that the factory is NOT called.
  // There is no way to have a callback for "function is not called", so we need
  // to use `RunUntilIdle` here.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(mojom_factory.TakeLastTestController());
}

TEST_F(PickerLacrosOmniboxSearchProviderTest,
       ControllerSendsToRemoteControllerFromFactory) {
  base::test::SingleThreadTaskEnvironment environment;
  crosapi::SearchControllerFactoryAsh factory;
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());
  PickerLacrosOmniboxSearchProvider provider(&factory, false, false, false);
  crosapi::SearchControllerAsh* controller = provider.GetController();
  ASSERT_TRUE(controller);
  mojom_factory.RunUntilCreateSearchController();
  std::unique_ptr<TestMojomSearchController> mojom_controller =
      mojom_factory.TakeLastTestController();
  ASSERT_TRUE(mojom_controller);

  controller->Search(u"cat", base::DoNothing());
  mojom_controller->RunUntilSearch();

  EXPECT_EQ(mojom_controller->last_query(), u"cat");
}

TEST_F(PickerLacrosOmniboxSearchProviderTest,
       RemoteControllerHasCorrectProviderTypes) {
  base::test::SingleThreadTaskEnvironment environment;
  crosapi::SearchControllerFactoryAsh factory;
  TestMojomSearchControllerFactory mojom_factory;
  factory.BindRemote(mojom_factory.BindToRemote());

  PickerLacrosOmniboxSearchProvider provider(
      &factory, /*bookmarks=*/true, /*history=*/false, /*open_tabs=*/true);
  mojom_factory.RunUntilCreateSearchController();

  std::unique_ptr<TestMojomSearchController> mojom_controller =
      mojom_factory.TakeLastTestController();
  ASSERT_TRUE(mojom_controller);
  EXPECT_TRUE(mojom_controller->bookmarks());
  EXPECT_FALSE(mojom_controller->history());
  EXPECT_TRUE(mojom_controller->open_tabs());
}

}  // namespace
