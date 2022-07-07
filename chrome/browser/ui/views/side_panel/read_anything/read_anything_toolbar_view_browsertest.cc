// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::IsFalse;
using testing::IsTrue;

class MockReadAnythingToolbarViewDelegate
    : public ReadAnythingToolbarView::Delegate {
 public:
  MOCK_METHOD(void, OnFontChoiceChanged, (int new_choice), (override));
  MOCK_METHOD(void, OnFontSizeChanged, (bool increase), (override));
};

class MockReadAnythingCoordinator : public ReadAnythingCoordinator {
 public:
  explicit MockReadAnythingCoordinator(Browser* browser)
      : ReadAnythingCoordinator(browser) {}

  MOCK_METHOD(void,
              CreateAndRegisterEntry,
              (SidePanelRegistry * global_registry));
  MOCK_METHOD(ReadAnythingController*, GetController, ());
  MOCK_METHOD(ReadAnythingModel*, GetModel, ());
  MOCK_METHOD(void,
              AddObserver,
              (ReadAnythingCoordinator::Observer * observer));
  MOCK_METHOD(void,
              RemoveObserver,
              (ReadAnythingCoordinator::Observer * observer));
};

class ReadAnythingToolbarViewTest : public InProcessBrowserTest {
 public:
  ReadAnythingToolbarViewTest() = default;
  ~ReadAnythingToolbarViewTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    coordinator_ = std::make_unique<MockReadAnythingCoordinator>(browser());

    toolbar_view_ = std::make_unique<ReadAnythingToolbarView>(
        coordinator_.get(), &delegate_);
  }

  void TearDownOnMainThread() override { coordinator_ = nullptr; }

  // Wrapper methods around the ReadAnythingToolbarView.

  void FontNameChangedCallback() { toolbar_view_->FontNameChangedCallback(); }

  void DecreaseFontSizeCallback() { toolbar_view_->DecreaseFontSizeCallback(); }

  void IncreaseFontSizeCallback() { toolbar_view_->IncreaseFontSizeCallback(); }

 protected:
  MockReadAnythingToolbarViewDelegate delegate_;

 private:
  std::unique_ptr<ReadAnythingToolbarView> toolbar_view_;
  std::unique_ptr<MockReadAnythingCoordinator> coordinator_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest, FontNameChanged) {
  EXPECT_CALL(delegate_, OnFontChoiceChanged(_)).Times(1);
  EXPECT_CALL(delegate_, OnFontSizeChanged(_)).Times(0);

  FontNameChangedCallback();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest, DecreaseTextSizeClicked) {
  EXPECT_CALL(delegate_, OnFontChoiceChanged(_)).Times(0);
  EXPECT_CALL(delegate_, OnFontSizeChanged(IsFalse())).Times(1);

  DecreaseFontSizeCallback();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest, IncreaseTextSizeClicked) {
  EXPECT_CALL(delegate_, OnFontChoiceChanged(_)).Times(0);
  EXPECT_CALL(delegate_, OnFontSizeChanged(IsTrue())).Times(1);

  IncreaseFontSizeCallback();
}
