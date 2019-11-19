// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_list.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views_test_api.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "components/web_modal/test_web_contents_modal_dialog_host.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

using content::DesktopMediaID;

namespace views {

class MockDesktopMediaPickerDialogObserver
    : public DesktopMediaPickerManager::DialogObserver {
 public:
  MOCK_METHOD0(OnDialogOpened, void());
  MOCK_METHOD0(OnDialogClosed, void());
};

const std::vector<DesktopMediaID::Type> kSourceTypes = {
    DesktopMediaID::TYPE_SCREEN, DesktopMediaID::TYPE_WINDOW,
    DesktopMediaID::TYPE_WEB_CONTENTS};

class DesktopMediaPickerViewsTest : public testing::Test {
 public:
  DesktopMediaPickerViewsTest() : source_types_(kSourceTypes) {}
  explicit DesktopMediaPickerViewsTest(
      const std::vector<DesktopMediaID::Type>& source_types)
      : source_types_(source_types) {}
  ~DesktopMediaPickerViewsTest() override {}

  void SetUp() override {
    test_helper_.test_views_delegate()->set_layout_provider(
        ChromeLayoutProvider::CreateLayoutProvider());

#if defined(OS_MACOSX)
    // These tests create actual child Widgets, which normally have a closure
    // animation on Mac; inhibit it here to avoid the tests flakily hanging.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableModalAnimations);
#endif

    std::vector<std::unique_ptr<DesktopMediaList>> source_lists;
    for (auto type : source_types_) {
      media_lists_[type] = new FakeDesktopMediaList(type);
      source_lists.push_back(
          std::unique_ptr<FakeDesktopMediaList>(media_lists_[type]));
    }

    base::string16 app_name = base::ASCIIToUTF16("foo");

    picker_views_ = std::make_unique<DesktopMediaPickerViews>();
    test_api_.set_picker(picker_views_.get());
    DesktopMediaPicker::Params picker_params;
    picker_params.context = test_helper_.GetContext();
    picker_params.app_name = app_name;
    picker_params.target_name = app_name;
    picker_params.request_audio = true;
    DesktopMediaPickerManager::Get()->AddObserver(&observer_);
    EXPECT_CALL(observer_, OnDialogOpened());
    EXPECT_CALL(observer_, OnDialogClosed());
    picker_views_->Show(
        picker_params, std::move(source_lists),
        base::BindOnce(&DesktopMediaPickerViewsTest::OnPickerDone,
                       base::Unretained(this)));
  }

  void TearDown() override {
    if (GetPickerDialogView()) {
      EXPECT_CALL(*this, OnPickerDone(content::DesktopMediaID()));
      GetPickerDialogView()->GetWidget()->CloseNow();
    }
    DesktopMediaPickerManager::Get()->RemoveObserver(&observer_);
  }

  DesktopMediaPickerDialogView* GetPickerDialogView() const {
    return picker_views_->GetDialogViewForTesting();
  }

  MOCK_METHOD1(OnPickerDone, void(content::DesktopMediaID));

 protected:
  content::BrowserTaskEnvironment task_environment_;
  views::ScopedViewsTestHelper test_helper_;
  std::map<DesktopMediaID::Type, FakeDesktopMediaList*> media_lists_;
  std::unique_ptr<DesktopMediaPickerViews> picker_views_;
  DesktopMediaPickerViewsTestApi test_api_;
  MockDesktopMediaPickerDialogObserver observer_;
  const std::vector<DesktopMediaID::Type> source_types_;
};

TEST_F(DesktopMediaPickerViewsTest, DoneCallbackCalledWhenWindowClosed) {
  EXPECT_CALL(*this, OnPickerDone(content::DesktopMediaID()));

  GetPickerDialogView()->GetWidget()->Close();
  base::RunLoop().RunUntilIdle();
}

TEST_F(DesktopMediaPickerViewsTest, DoneCallbackCalledOnOkButtonPressed) {
  const DesktopMediaID kFakeId(DesktopMediaID::TYPE_WINDOW, 222);
  EXPECT_CALL(*this, OnPickerDone(kFakeId));

  media_lists_[DesktopMediaID::TYPE_WINDOW]->AddSourceByFullMediaID(kFakeId);
  test_api_.GetAudioShareCheckbox()->SetChecked(true);

  EXPECT_FALSE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  test_api_.SelectTabForSourceType(DesktopMediaID::TYPE_WINDOW);
  test_api_.FocusSourceAtIndex(0);

  EXPECT_TRUE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  GetPickerDialogView()->GetDialogClientView()->AcceptWindow();
  base::RunLoop().RunUntilIdle();
}

// Verifies that a MediaSourceView is selected with mouse left click and
// original selected MediaSourceView gets unselected.
TEST_F(DesktopMediaPickerViewsTest, SelectMediaSourceViewOnSingleClick) {
  for (auto source_type : kSourceTypes) {
    test_api_.SelectTabForSourceType(source_type);
    media_lists_[source_type]->AddSourceByFullMediaID(
        DesktopMediaID(source_type, 10));
    media_lists_[source_type]->AddSourceByFullMediaID(
        DesktopMediaID(source_type, 20));

    // By default, nothing should be selected.
    EXPECT_FALSE(test_api_.GetSelectedSourceId().has_value());

    test_api_.PressMouseOnSourceAtIndex(0);
    ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
    EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());

    test_api_.PressMouseOnSourceAtIndex(1);
    ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
    EXPECT_EQ(20, test_api_.GetSelectedSourceId().value());
  }
}

TEST_F(DesktopMediaPickerViewsTest, DoneCallbackCalledOnDoubleClick) {
  const DesktopMediaID kFakeId(DesktopMediaID::TYPE_WEB_CONTENTS, 222);
  EXPECT_CALL(*this, OnPickerDone(kFakeId));

  media_lists_[DesktopMediaID::TYPE_WEB_CONTENTS]->AddSourceByFullMediaID(
      kFakeId);
  test_api_.SelectTabForSourceType(DesktopMediaID::TYPE_WEB_CONTENTS);
  test_api_.GetAudioShareCheckbox()->SetChecked(false);

  test_api_.PressMouseOnSourceAtIndex(0, true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DesktopMediaPickerViewsTest, DoneCallbackCalledOnDoubleTap) {
  const DesktopMediaID kFakeId(DesktopMediaID::TYPE_SCREEN, 222);
  EXPECT_CALL(*this, OnPickerDone(kFakeId));

  test_api_.SelectTabForSourceType(DesktopMediaID::TYPE_SCREEN);
  test_api_.GetAudioShareCheckbox()->SetChecked(false);

  media_lists_[DesktopMediaID::TYPE_SCREEN]->AddSourceByFullMediaID(kFakeId);
  test_api_.DoubleTapSourceAtIndex(0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DesktopMediaPickerViewsTest, CancelButtonAlwaysEnabled) {
  EXPECT_TRUE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

// Verifies that the MediaSourceView is added or removed when |media_list_| is
// updated.
TEST_F(DesktopMediaPickerViewsTest, AddAndRemoveMediaSource) {
  for (auto source_type : kSourceTypes) {
    test_api_.SelectTabForSourceType(source_type);
    // No media source at first.
    EXPECT_FALSE(test_api_.HasSourceAtIndex(0));

    for (int i = 0; i < 3; ++i) {
      media_lists_[source_type]->AddSourceByFullMediaID(
          DesktopMediaID(source_type, i));
      EXPECT_TRUE(test_api_.HasSourceAtIndex(i));
    }

    for (int i = 2; i >= 0; --i) {
      media_lists_[source_type]->RemoveSource(i);
      EXPECT_FALSE(test_api_.HasSourceAtIndex(i));
    }
  }
}

// Verifies that focusing the MediaSourceView marks it selected and the
// original selected MediaSourceView gets unselected.
TEST_F(DesktopMediaPickerViewsTest, FocusMediaSourceViewToSelect) {
  for (auto source_type : kSourceTypes) {
    test_api_.SelectTabForSourceType(source_type);
    media_lists_[source_type]->AddSourceByFullMediaID(
        DesktopMediaID(source_type, 10));
    media_lists_[source_type]->AddSourceByFullMediaID(
        DesktopMediaID(source_type, 20));

    test_api_.FocusSourceAtIndex(0);
    ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
    EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());

    test_api_.FocusAudioCheckbox();
    ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
    EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());

    test_api_.FocusSourceAtIndex(1);
    ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
    EXPECT_EQ(20, test_api_.GetSelectedSourceId().value());
  }
}

TEST_F(DesktopMediaPickerViewsTest, OkButtonDisabledWhenNoSelection) {
  for (auto source_type : kSourceTypes) {
    test_api_.SelectTabForSourceType(source_type);
    media_lists_[source_type]->AddSourceByFullMediaID(
        DesktopMediaID(source_type, 111));

    test_api_.FocusSourceAtIndex(0);
    EXPECT_TRUE(
        GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

    media_lists_[source_type]->RemoveSource(0);
    EXPECT_FALSE(
        GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  }
}

// Verifies the visible status of audio checkbox.
TEST_F(DesktopMediaPickerViewsTest, AudioCheckboxState) {
  bool expect_value = false;
  test_api_.SelectTabForSourceType(DesktopMediaID::TYPE_SCREEN);
#if defined(OS_WIN) || defined(USE_CRAS)
  expect_value = true;
#endif
  EXPECT_EQ(expect_value, test_api_.GetAudioShareCheckbox()->GetVisible());

  test_api_.SelectTabForSourceType(DesktopMediaID::TYPE_WINDOW);
  EXPECT_FALSE(test_api_.GetAudioShareCheckbox()->GetVisible());

  test_api_.SelectTabForSourceType(DesktopMediaID::TYPE_WEB_CONTENTS);
  EXPECT_TRUE(test_api_.GetAudioShareCheckbox()->GetVisible());
}

// Verifies that audio share information is recorded in the ID if the checkbox
// is checked.
TEST_F(DesktopMediaPickerViewsTest, DoneWithAudioShare) {
  DesktopMediaID originId(DesktopMediaID::TYPE_WEB_CONTENTS, 222);
  DesktopMediaID returnId = originId;
  returnId.audio_share = true;

  // This matches the real workflow that when a source is generated in
  // media_list, its |audio_share| bit is not set. The bit is set by the picker
  // UI if the audio checkbox is checked.
  EXPECT_CALL(*this, OnPickerDone(returnId));
  media_lists_[DesktopMediaID::TYPE_WEB_CONTENTS]->AddSourceByFullMediaID(
      originId);

  test_api_.SelectTabForSourceType(DesktopMediaID::TYPE_WEB_CONTENTS);
  test_api_.GetAudioShareCheckbox()->SetChecked(true);
  test_api_.FocusSourceAtIndex(0);

  GetPickerDialogView()->GetDialogClientView()->AcceptWindow();
  base::RunLoop().RunUntilIdle();
}

// Creates a single pane DesktopMediaPickerViews that only has a tab list.
class DesktopMediaPickerViewsSingleTabPaneTest
    : public DesktopMediaPickerViewsTest {
 public:
  DesktopMediaPickerViewsSingleTabPaneTest()
      : DesktopMediaPickerViewsTest({DesktopMediaID::TYPE_WEB_CONTENTS}) {}
  ~DesktopMediaPickerViewsSingleTabPaneTest() override {}
};

// Validates that the tab list's preferred size is not zero
// (https://crbug.com/965408).
TEST_F(DesktopMediaPickerViewsSingleTabPaneTest, TabListPreferredSizeNotZero) {
  EXPECT_GT(test_api_.GetSelectedListView()->height(), 0);
}

// Validates that the tab list has a fixed height (https://crbug.com/998485).
TEST_F(DesktopMediaPickerViewsSingleTabPaneTest, TabListHasFixedHeight) {
  auto AddTabSource = [&]() {
    media_lists_[DesktopMediaID::TYPE_WEB_CONTENTS]->AddSourceByFullMediaID(
        DesktopMediaID(DesktopMediaID::TYPE_WEB_CONTENTS, 0));
  };

  auto GetDialogHeight = [&]() {
    return GetPickerDialogView()
        ->GetDialogClientView()
        ->GetPreferredSize()
        .height();
  };

  int initial_size = GetDialogHeight();

  // The dialog's height should not change when going from zero sources to nine
  // sources.
  for (int i = 0; i < 9; i++)
    AddTabSource();
  EXPECT_EQ(GetDialogHeight(), initial_size);

  // The dialog's height should be fixed and equal to the equivalent of ten
  // rows, thus it should not change when going from nine to eleven either.
  AddTabSource();
  EXPECT_EQ(GetDialogHeight(), initial_size);
  AddTabSource();
  EXPECT_EQ(GetDialogHeight(), initial_size);

  // And then it shouldn't change when going to a larger number of sources.
  for (int i = 0; i < 50; i++)
    AddTabSource();
  EXPECT_EQ(GetDialogHeight(), initial_size);

  // And then it shouldn't change when going from a large number of sources (in
  // this case 61) to a larger number, because the ScrollView should scroll
  // large numbers of sources.
  for (int i = 0; i < 50; i++)
    AddTabSource();
  EXPECT_EQ(GetDialogHeight(), initial_size);
}

}  // namespace views
