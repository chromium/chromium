// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// chrome::AddAndReturnTabAt is not easily mockable. For now, use a wrapper
// class and abstract away the method such that the functionality of
// TabStripServiceImpl can still be tested.
class MockTabStripServiceImpl : public TabStripServiceImpl {
 public:
  MockTabStripServiceImpl(BrowserWindowInterface* browser,
                          TabStripModel* tab_strip_model)
      : TabStripServiceImpl(browser, tab_strip_model) {}
  MockTabStripServiceImpl(const MockTabStripServiceImpl&) = delete;
  MockTabStripServiceImpl operator=(const MockTabStripServiceImpl&) = delete;
  ~MockTabStripServiceImpl() override = default;

 protected:
  content::WebContents* AddTabAt(const GURL& url, int index) override {
    // Integrate with chrome::AddTabAt and add success case.
    return nullptr;
  }
};

class TabStripServiceImplTest : public testing::Test {
 protected:
  TabStripServiceImplTest()
      : profile_(std::make_unique<TestingProfile>()),
        delegate_(std::make_unique<TestTabStripModelDelegate>()),
        tab_strip_model_(
            std::make_unique<TabStripModel>(delegate(), profile())),
        browser_window_interface_(
            std::make_unique<MockBrowserWindowInterface>()) {}
  TabStripServiceImplTest(const TabStripServiceImplTest&) = delete;
  TabStripServiceImplTest operator=(const TabStripServiceImplTest&) = delete;
  ~TabStripServiceImplTest() override = default;

  void SetUp() override {
    impl_ = std::make_unique<MockTabStripServiceImpl>(
        browser_window_interface(), tab_strip_model());
    impl_->Accept(client_.BindNewPipeAndPassReceiver());
  }

  TestingProfile* profile() { return profile_.get(); }
  TestTabStripModelDelegate* delegate() { return delegate_.get(); }
  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }
  MockBrowserWindowInterface* browser_window_interface() {
    return browser_window_interface_.get();
  }

  mojo::Remote<tabs_api::mojom::TabStripService> client_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestTabStripModelDelegate> delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<TabStripServiceImpl> impl_;
};

TEST_F(TabStripServiceImplTest, CreateNewTab) {
  tabs_api::mojom::TabStripService::CreateTabAtResult result;
  bool success = client_->CreateTabAt(nullptr, std::nullopt, &result);

  ASSERT_TRUE(success);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(result.error()->code, mojo_base::mojom::Code::kFailedPrecondition);
}

}  // namespace
