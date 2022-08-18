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
  MOCK_METHOD(void, OnFontSizeChanged, (bool increase), (override));
  MOCK_METHOD(void, OnColorsChanged, (int new_index), (override));
  MOCK_METHOD(ui::ComboboxModel*, GetColorsModel, (), (override));
};

class MockReadAnythingFontComboboxDelegate
    : public ReadAnythingFontCombobox::Delegate {
 public:
  MOCK_METHOD(void, OnFontChoiceChanged, (int new_index), (override));
  MOCK_METHOD(ui::ComboboxModel*, GetFontComboboxModel, (), (override));
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
        coordinator_.get(), &toolbar_delegate_, &font_combobox_delegate_);
  }

  void TearDownOnMainThread() override { coordinator_ = nullptr; }

  // Wrapper methods around the ReadAnythingToolbarView.

  void DecreaseFontSizeCallback() { toolbar_view_->DecreaseFontSizeCallback(); }

  void IncreaseFontSizeCallback() { toolbar_view_->IncreaseFontSizeCallback(); }

  void ChangeColorsCallback() { toolbar_view_->ChangeColorsCallback(); }

 protected:
  MockReadAnythingToolbarViewDelegate toolbar_delegate_;
  MockReadAnythingFontComboboxDelegate font_combobox_delegate_;

 private:
  std::unique_ptr<ReadAnythingToolbarView> toolbar_view_;
  std::unique_ptr<MockReadAnythingCoordinator> coordinator_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest, DecreaseFontSizeCallback) {
  EXPECT_CALL(toolbar_delegate_, OnFontSizeChanged(false)).Times(1);
  EXPECT_CALL(toolbar_delegate_, OnFontSizeChanged(true)).Times(0);

  DecreaseFontSizeCallback();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest, IncreaseFontSizeCallback) {
  EXPECT_CALL(toolbar_delegate_, OnFontSizeChanged(false)).Times(0);
  EXPECT_CALL(toolbar_delegate_, OnFontSizeChanged(true)).Times(1);

  IncreaseFontSizeCallback();
}

IN_PROC_BROWSER_TEST_F(ReadAnythingToolbarViewTest, ChangeColorsCallback) {
  EXPECT_CALL(toolbar_delegate_, OnColorsChanged(0)).Times(1);

  ChangeColorsCallback();
}
