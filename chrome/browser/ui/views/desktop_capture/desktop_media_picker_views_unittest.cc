// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_list.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views_test_api.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
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
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/projector_session.h"
#endif

using content::DesktopMediaID;

namespace views {

class TestDialogObserver : public DesktopMediaPickerManager::DialogObserver {
 public:
  ~TestDialogObserver() override {
    EXPECT_TRUE(opened_);
    EXPECT_TRUE(closed_);
  }

 private:
  void OnDialogOpened() override { opened_ = true; }
  void OnDialogClosed() override { closed_ = true; }

  bool opened_ = false;
  bool closed_ = false;
};

const std::vector<DesktopMediaList::Type> kSourceTypes = {
    DesktopMediaList::Type::kScreen, DesktopMediaList::Type::kWindow,
    DesktopMediaList::Type::kWebContents};

const std::vector<DesktopMediaID::Type> kSourceIdTypes = {
    DesktopMediaID::TYPE_SCREEN, DesktopMediaID::TYPE_WINDOW,
    DesktopMediaID::TYPE_WEB_CONTENTS};

class DesktopMediaPickerViewsTest : public testing::Test {
 public:
  DesktopMediaPickerViewsTest() : source_types_(kSourceTypes) {}
  explicit DesktopMediaPickerViewsTest(
      const std::vector<DesktopMediaList::Type>& source_types)
      : source_types_(source_types) {}
  ~DesktopMediaPickerViewsTest() override = default;

  void SetUp() override {
#if defined(OS_MAC)
    // These tests create actual child Widgets, which normally have a closure
    // animation on Mac; inhibit it here to avoid the tests flakily hanging.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableModalAnimations);
#endif
    DesktopMediaPickerManager::Get()->AddObserver(&observer_);
    CreatePickerViews();
  }

  void TearDown() override {
    if (GetPickerDialogView())
      GetPickerDialogView()->GetWidget()->CloseNow();
    widget_destroyed_waiter_->Wait();
    DesktopMediaPickerManager::Get()->RemoveObserver(&observer_);
  }

  void CreatePickerViews() {
    widget_destroyed_waiter_.reset();
    picker_views_.reset();

    picker_views_ = std::make_unique<DesktopMediaPickerViews>();
    test_api_.set_picker(picker_views_.get());

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "DesktopMediaPickerDialogView");

    const std::u16string kAppName = u"foo";
    DesktopMediaPicker::Params picker_params;
    picker_params.context = test_helper_.GetContext();
    picker_params.app_name = kAppName;
    picker_params.target_name = kAppName;
    picker_params.request_audio = true;

    std::vector<std::unique_ptr<DesktopMediaList>> source_lists;
    for (auto type : source_types_) {
      source_lists.push_back(std::make_unique<FakeDesktopMediaList>(type));
      media_lists_[type] =
          static_cast<FakeDesktopMediaList*>(source_lists.back().get());
    }

    picker_views_->Show(
        picker_params, std::move(source_lists),
        base::BindOnce(&DesktopMediaPickerViewsTest::OnPickerDone,
                       base::Unretained(this)));
    widget_destroyed_waiter_ =
        std::make_unique<views::test::WidgetDestroyedWaiter>(
            waiter.WaitIfNeededAndGet());
  }

  DesktopMediaPickerDialogView* GetPickerDialogView() const {
    return picker_views_->GetDialogViewForTesting();
  }

  void OnPickerDone(content::DesktopMediaID picked_id) {
    picked_id_ = picked_id;
    run_loop_.Quit();
  }

  base::Optional<content::DesktopMediaID> WaitForPickerDone() {
    run_loop_.Run();
    return picked_id_;
  }

  base::Optional<content::DesktopMediaID> picked_id() const {
    return picked_id_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  views::ScopedViewsTestHelper test_helper_{
      std::make_unique<ChromeTestViewsDelegate<>>()};
  std::map<DesktopMediaList::Type, FakeDesktopMediaList*> media_lists_;
  std::unique_ptr<DesktopMediaPickerViews> picker_views_;
  DesktopMediaPickerViewsTestApi test_api_;
  TestDialogObserver observer_;
  const std::vector<DesktopMediaList::Type> source_types_;

  base::RunLoop run_loop_;
  base::Optional<content::DesktopMediaID> picked_id_;
  std::unique_ptr<views::test::WidgetDestroyedWaiter> widget_destroyed_waiter_;
};

class DesktopMediaPickerDoubleClickTest
    : public DesktopMediaPickerViewsTest,
      public testing::WithParamInterface<
          std::pair<DesktopMediaList::Type, DesktopMediaID::Type>> {
 public:
  DesktopMediaPickerDoubleClickTest() = default;
};

// Regression test for https://crbug.com/1102153 and https://crbug.com/1127496
TEST_P(DesktopMediaPickerDoubleClickTest, DoneCallbackNotCalledOnDoubleClick) {
  const DesktopMediaList::Type media_list_type = std::get<0>(GetParam());
  const DesktopMediaID::Type media_type = std::get<1>(GetParam());

  const DesktopMediaID kFakeId(media_type, 222);

  media_lists_[media_list_type]->AddSourceByFullMediaID(kFakeId);
  test_api_.SelectTabForSourceType(media_list_type);
  test_api_.PressMouseOnSourceAtIndex(0, true);

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(picked_id().has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DesktopMediaPickerDoubleClickTest,
    testing::Values(std::make_pair(DesktopMediaList::Type::kWindow,
                                   DesktopMediaID::TYPE_WINDOW),
                    std::make_pair(DesktopMediaList::Type::kScreen,
                                   DesktopMediaID::TYPE_SCREEN),
                    std::make_pair(DesktopMediaList::Type::kWebContents,
                                   DesktopMediaID::TYPE_WEB_CONTENTS)));

TEST_F(DesktopMediaPickerViewsTest, DoneCallbackCalledWhenWindowClosed) {
  GetPickerDialogView()->GetWidget()->Close();
  EXPECT_EQ(content::DesktopMediaID(), WaitForPickerDone());
}

TEST_F(DesktopMediaPickerViewsTest, DoneCallbackCalledOnOkButtonPressed) {
  const DesktopMediaID kFakeId(DesktopMediaID::TYPE_WINDOW, 222);

  media_lists_[DesktopMediaList::Type::kWindow]->AddSourceByFullMediaID(
      kFakeId);
  test_api_.GetAudioShareCheckbox()->SetChecked(true);

  EXPECT_FALSE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
  test_api_.FocusSourceAtIndex(0);

  EXPECT_TRUE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  GetPickerDialogView()->AcceptDialog();
  EXPECT_EQ(kFakeId, WaitForPickerDone());
}

// Verifies that a MediaSourceView is selected with mouse left click and
// original selected MediaSourceView gets unselected.
TEST_F(DesktopMediaPickerViewsTest, SelectMediaSourceViewOnSingleClick) {
  for (size_t i = 0; i < kSourceTypes.size(); ++i) {
    const auto source_type = kSourceTypes[i];
    const auto source_id_type = kSourceIdTypes[i];

    test_api_.SelectTabForSourceType(source_type);
    media_lists_[source_type]->AddSourceByFullMediaID(
        DesktopMediaID(source_id_type, 10));
    media_lists_[source_type]->AddSourceByFullMediaID(
        DesktopMediaID(source_id_type, 20));

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

// Regression test for https://crbug.com/1102153
TEST_F(DesktopMediaPickerViewsTest, DoneCallbackNotCalledOnDoubleTap) {
  const DesktopMediaID kFakeId(DesktopMediaID::TYPE_SCREEN, 222);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  test_api_.GetAudioShareCheckbox()->SetChecked(false);

  media_lists_[DesktopMediaList::Type::kScreen]->AddSourceByFullMediaID(
      kFakeId);
  test_api_.DoubleTapSourceAtIndex(0);
  EXPECT_FALSE(picked_id().has_value());
}

TEST_F(DesktopMediaPickerViewsTest, CancelButtonAlwaysEnabled) {
  EXPECT_TRUE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

// Verifies that the MediaSourceView is added or removed when |media_list_| is
// updated.
TEST_F(DesktopMediaPickerViewsTest, AddAndRemoveMediaSource) {
  for (size_t j = 0; j < kSourceTypes.size(); j++) {
    const auto source_type = kSourceTypes[j];
    const auto source_id_type = kSourceIdTypes[j];

    test_api_.SelectTabForSourceType(source_type);
    // No media source at first.
    EXPECT_FALSE(test_api_.HasSourceAtIndex(0));

    for (int i = 0; i < 3; ++i) {
      media_lists_[source_type]->AddSourceByFullMediaID(
          DesktopMediaID(source_id_type, i));
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
  for (size_t i = 0; i < kSourceIdTypes.size(); ++i) {
    const auto source_type = kSourceTypes[i];
    const auto source_id_type = kSourceIdTypes[i];

    test_api_.SelectTabForSourceType(source_type);
    media_lists_[source_type]->AddSourceByFullMediaID(
        DesktopMediaID(source_id_type, 10));
    media_lists_[source_type]->AddSourceByFullMediaID(
        DesktopMediaID(source_id_type, 20));

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
  for (size_t i = 0; i < kSourceIdTypes.size(); ++i) {
    const auto source_type = kSourceTypes[i];
    const auto source_id_type = kSourceIdTypes[i];

    test_api_.SelectTabForSourceType(source_type);
    media_lists_[source_type]->AddSourceByFullMediaID(
        DesktopMediaID(source_id_type, 111));
    EXPECT_FALSE(
        GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

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
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  EXPECT_EQ(DesktopMediaPickerViews::kScreenAudioShareSupportedOnPlatform,
            test_api_.GetAudioShareCheckbox()->GetVisible());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
  EXPECT_FALSE(test_api_.GetAudioShareCheckbox()->GetVisible());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  EXPECT_TRUE(test_api_.GetAudioShareCheckbox()->GetVisible());
}

// Verifies that audio share information is recorded in the ID if the checkbox
// is checked.
TEST_F(DesktopMediaPickerViewsTest, DoneWithAudioShare) {
  constexpr DesktopMediaID kOriginId(DesktopMediaID::TYPE_WEB_CONTENTS, 222);
  constexpr DesktopMediaID kResultId(DesktopMediaID::TYPE_WEB_CONTENTS, 222,
                                     true);

  // This matches the real workflow that when a source is generated in
  // media_list, its |audio_share| bit is not set. The bit is set by the picker
  // UI if the audio checkbox is checked.
  media_lists_[DesktopMediaList::Type::kWebContents]->AddSourceByFullMediaID(
      kOriginId);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  test_api_.GetAudioShareCheckbox()->SetChecked(true);
  test_api_.FocusSourceAtIndex(0);

  GetPickerDialogView()->AcceptDialog();
  EXPECT_EQ(kResultId, WaitForPickerDone());
}

TEST_F(DesktopMediaPickerViewsTest, OkButtonEnabledDuringAcceptSpecific) {
  constexpr DesktopMediaID kFakeId(
      DesktopMediaID::TYPE_SCREEN, 222,
      DesktopMediaPickerViews::kScreenAudioShareSupportedOnPlatform);

  media_lists_[DesktopMediaList::Type::kWindow]->AddSourceByFullMediaID(
      kFakeId);

  EXPECT_FALSE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  GetPickerDialogView()->AcceptSpecificSource(kFakeId);
  EXPECT_EQ(kFakeId, WaitForPickerDone());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class ProjectorControllerMock : public ash::ProjectorController {
 public:
  ProjectorControllerMock() = default;
  ~ProjectorControllerMock() = default;
  MOCK_METHOD1(SetClient, void(ash::ProjectorClient*));
  MOCK_METHOD1(OnSpeechRecognitionAvailable, void(bool));
  MOCK_METHOD5(OnTranscription,
               void(const std::u16string&,
                    base::TimeDelta,
                    base::TimeDelta,
                    const std::vector<base::TimeDelta>&,
                    bool));
  MOCK_METHOD1(SetProjectorToolsVisible, void(bool));
  MOCK_METHOD2(StartProjectorSession,
               void(ash::SourceType scope, aura::Window* window));
  MOCK_CONST_METHOD0(IsEligible, bool());
};

TEST_F(DesktopMediaPickerViewsTest, ProjectorIneligible) {
  ProjectorControllerMock mock;
  ON_CALL(mock, IsEligible).WillByDefault(testing::Return(false));
  EXPECT_EQ(test_api_.GetPresenterToolsCheckbox(), nullptr);
}

TEST_F(DesktopMediaPickerViewsTest, ProjectorEligible) {
  ProjectorControllerMock mock;
  ON_CALL(mock, IsEligible).WillByDefault(testing::Return(true));

  EXPECT_CALL(mock, SetProjectorToolsVisible(true)).Times(1);
  CreatePickerViews();

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  EXPECT_TRUE(test_api_.GetPresenterToolsCheckbox()->GetVisible());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
  EXPECT_TRUE(test_api_.GetPresenterToolsCheckbox()->GetVisible());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  EXPECT_TRUE(test_api_.GetPresenterToolsCheckbox()->GetVisible());

  // Button is visible but unchecked.
  test_api_.GetPresenterToolsCheckbox()->SetChecked(true);

  // When the dialog is cancelled, we expect that ProjectorController to hide
  // the toolbar.
  EXPECT_CALL(mock, SetProjectorToolsVisible(false)).Times(1);

  // Cancel and dismiss the Dialog
  GetPickerDialogView()->Cancel();
}

TEST_F(DesktopMediaPickerViewsTest, ProjectorStartSession) {
  constexpr DesktopMediaID kOriginId(DesktopMediaID::TYPE_WEB_CONTENTS, 222);

  ProjectorControllerMock mock;
  ON_CALL(mock, IsEligible).WillByDefault(testing::Return(true));
  CreatePickerViews();

  media_lists_[DesktopMediaList::Type::kWebContents]->AddSourceByFullMediaID(
      kOriginId);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  test_api_.GetPresenterToolsCheckbox()->SetChecked(true);
  test_api_.FocusSourceAtIndex(0);

  EXPECT_CALL(mock, StartProjectorSession(ash::SourceType::kTab, testing::_))
      .Times(1);

  GetPickerDialogView()->AcceptDialog();
  WaitForPickerDone();
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Creates a single pane DesktopMediaPickerViews that only has a tab list.
class DesktopMediaPickerViewsSingleTabPaneTest
    : public DesktopMediaPickerViewsTest {
 public:
  DesktopMediaPickerViewsSingleTabPaneTest()
      : DesktopMediaPickerViewsTest({DesktopMediaList::Type::kWebContents}) {}
  ~DesktopMediaPickerViewsSingleTabPaneTest() override = default;

 protected:
  void AddTabSource() {
    media_lists_[DesktopMediaList::Type::kWebContents]->AddSourceByFullMediaID(
        DesktopMediaID(DesktopMediaID::TYPE_WEB_CONTENTS, 0));
  }
};

// Validates that the tab list's preferred size is not zero
// (https://crbug.com/965408).
TEST_F(DesktopMediaPickerViewsSingleTabPaneTest, TabListPreferredSizeNotZero) {
  EXPECT_GT(test_api_.GetSelectedListView()->height(), 0);
}

// Validates that the tab list has a fixed height (https://crbug.com/998485).
TEST_F(DesktopMediaPickerViewsSingleTabPaneTest, TabListHasFixedHeight) {
  auto GetDialogHeight = [&]() {
    return GetPickerDialogView()->GetPreferredSize().height();
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

// Regression test for https://crbug.com/1042976.
TEST_F(DesktopMediaPickerViewsSingleTabPaneTest,
       CannotAcceptTabWithoutSelection) {
  AddTabSource();
  AddTabSource();
  AddTabSource();

  test_api_.FocusSourceAtIndex(0, false);
  EXPECT_EQ(base::nullopt, test_api_.GetSelectedSourceId());
  EXPECT_FALSE(
      GetPickerDialogView()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // Send the tab list a Return key press, to make sure it doesn't try to accept
  // with no selected source. If the fix to https://crbug.com/1042976 regresses,
  // this test will crash here.
  test_api_.PressKeyOnSourceAtIndex(
      0, ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0));
}

}  // namespace views
