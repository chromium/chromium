// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_utils.h"
#include "chrome/browser/media/webrtc/fake_desktop_media_list.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_delegated_source_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_controller.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_list_view.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_picker_views_test_api.h"
#include "chrome/browser/ui/views/desktop_capture/desktop_media_source_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/web_modal/test_web_contents_modal_dialog_host.h"
#include "content/public/test/browser_task_environment.h"
#include "media/audio/audio_features.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/scoped_views_test_helper.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

using ::blink::mojom::MediaStreamRequestResult;
using ::content::DesktopMediaID;

using PickedIdOrErrorCode =
    base::expected<content::DesktopMediaID,
                   blink::mojom::MediaStreamRequestResult>;

namespace views {

class TestDialogObserver : public DesktopMediaPickerManager::DialogObserver {
 public:
  ~TestDialogObserver() override {
    EXPECT_TRUE(opened_);
    EXPECT_TRUE(closed_);
  }

 private:
  void OnDialogOpened(const DesktopMediaPicker::Params&) override {
    opened_ = true;
  }
  void OnDialogClosed() override { closed_ = true; }

  bool opened_ = false;
  bool closed_ = false;
};

std::vector<DesktopMediaList::Type> GetSourceTypes(bool new_order) {
  if (new_order) {
    return {DesktopMediaList::Type::kWebContents,
            DesktopMediaList::Type::kWindow, DesktopMediaList::Type::kScreen};
  } else {
    return {DesktopMediaList::Type::kScreen, DesktopMediaList::Type::kWindow,
            DesktopMediaList::Type::kWebContents};
  }
}

std::string GetTypeAsTestNameString(const DesktopMediaList::Type type) {
  switch (type) {
    case DesktopMediaList::Type::kScreen:
      return "Screen";
    case DesktopMediaList::Type::kWindow:
      return "Window";
    case DesktopMediaList::Type::kWebContents:
      return "Tab";
    case DesktopMediaList::Type::kCurrentTab:
      NOTREACHED();
    case DesktopMediaList::Type::kNone:
      return "None";
  }
  NOTREACHED();
}

class DesktopMediaPickerViewsTestBase : public testing::Test {
 public:
  explicit DesktopMediaPickerViewsTestBase(
      const std::vector<DesktopMediaList::Type>& source_types)
      : source_types_(source_types) {}

  ~DesktopMediaPickerViewsTestBase() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_MAC)
    // These tests create actual child Widgets, which normally have a closure
    // animation on Mac; inhibit it here to avoid the tests flakily hanging.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableModalAnimations);
#endif
    DesktopMediaPickerManager::Get()->AddObserver(&observer_);
    MaybeCreatePickerViews();
  }

  virtual void MaybeCreatePickerViews() {
    CreatePickerViews(/*request_audio=*/true,
                      /*screen_exclude_system_audio=*/false,
                      blink::mojom::WindowAudioPreference::kExclude);
  }

  void TearDown() override {
    if (GetPickerDialogView()) {
      GetPickerDialogView()->GetWidget()->CloseNow();
    }
    widget_destroyed_waiter_->Wait();
    DesktopMediaPickerManager::Get()->RemoveObserver(&observer_);
  }

  void CreatePickerViews(
      bool request_audio,
      bool screen_exclude_system_audio,
      blink::mojom::WindowAudioPreference window_audio_preference,
      blink::mojom::PreferredDisplaySurface preferred_display_surface =
          blink::mojom::PreferredDisplaySurface::NO_PREFERENCE) {
    widget_destroyed_waiter_.reset();
    picker_views_.reset();

    picker_views_ = std::make_unique<DesktopMediaPickerImpl>();
    test_api_.set_picker(picker_views_.get());

    views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                         "DesktopMediaPickerDialogView");

    const std::u16string kAppName = u"foo";
    DesktopMediaPicker::Params picker_params{
        DesktopMediaPicker::Params::RequestSource::kUnknown};
    picker_params.context = test_helper_.GetContext();
    picker_params.app_name = kAppName;
    picker_params.target_name = kAppName;
    picker_params.request_audio = request_audio;
    picker_params.exclude_system_audio = screen_exclude_system_audio;
    picker_params.window_audio_preference = window_audio_preference;
    picker_params.preferred_display_surface = preferred_display_surface;

    std::vector<std::unique_ptr<DesktopMediaList>> source_lists;
    for (const DesktopMediaList::Type type : source_types_) {
      source_lists.push_back(std::make_unique<FakeDesktopMediaList>(type));
      media_lists_[type] =
          static_cast<FakeDesktopMediaList*>(source_lists.back().get());
    }

    for (const DesktopMediaList::Type type : delegated_source_types_) {
      source_lists.push_back(std::make_unique<FakeDesktopMediaList>(
          type, /*is_source_list_delegated=*/true));
      media_lists_[type] =
          static_cast<FakeDesktopMediaList*>(source_lists.back().get());
    }

    picker_views_->Show(
        picker_params, std::move(source_lists),
        base::BindOnce(&DesktopMediaPickerViewsTestBase::OnPickerDone,
                       weak_factory_.GetWeakPtr()));
    widget_destroyed_waiter_ =
        std::make_unique<views::test::WidgetDestroyedWaiter>(
            waiter.WaitIfNeededAndGet());
  }

  DesktopMediaPickerDialogView* GetPickerDialogView() const {
    return picker_views_->GetDialogViewForTesting();
  }

  void OnPickerDone(PickedIdOrErrorCode result) {
    picker_result_ = result;
    run_loop_.Quit();
  }

  PickedIdOrErrorCode WaitForPickerResult() {
    run_loop_.Run();
    CHECK(picker_result_.has_value());
    return picker_result_.value();
  }

  // Checks whether `picker_result_` was ever set.
  bool has_picker_result() const { return picker_result_.has_value(); }

  const std::vector<DesktopMediaList::Type>& source_types() {
    return source_types_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  views::ScopedViewsTestHelper test_helper_{
      std::make_unique<ChromeTestViewsDelegate<>>()};
  std::map<DesktopMediaList::Type,
           raw_ptr<FakeDesktopMediaList, CtnExperimental>>
      media_lists_;
  std::unique_ptr<DesktopMediaPickerImpl> picker_views_;
  DesktopMediaPickerViewsTestApi test_api_;
  TestDialogObserver observer_;
  std::vector<DesktopMediaList::Type> source_types_;
  std::vector<DesktopMediaList::Type> delegated_source_types_;

  base::RunLoop run_loop_;
  std::optional<PickedIdOrErrorCode> picker_result_;
  std::unique_ptr<views::test::WidgetDestroyedWaiter> widget_destroyed_waiter_;

  base::WeakPtrFactory<DesktopMediaPickerViewsTestBase> weak_factory_{this};
};

class DesktopMediaPickerViewsTest : public DesktopMediaPickerViewsTestBase,
                                    public testing::WithParamInterface<bool> {
 public:
  DesktopMediaPickerViewsTest()
      : DesktopMediaPickerViewsTestBase(GetSourceTypes(NewOrder())) {}
  ~DesktopMediaPickerViewsTest() override = default;

  bool NewOrder() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         DesktopMediaPickerViewsTest,
                         /*NewOrder=*/testing::Bool());

TEST_P(DesktopMediaPickerViewsTest, DoneCallbackCalledWhenWindowClosed) {
  GetPickerDialogView()->GetWidget()->Close();
  EXPECT_EQ(
      WaitForPickerResult(),
      base::unexpected(MediaStreamRequestResult::PERMISSION_DENIED_BY_USER));
}

TEST_P(DesktopMediaPickerViewsTest, DoneCallbackCalledOnOkButtonPressed) {
  const DesktopMediaID kFakeId(DesktopMediaID::TYPE_WINDOW, 222);

  media_lists_[DesktopMediaList::Type::kWindow]->AddSourceByFullMediaID(
      kFakeId);

  EXPECT_FALSE(GetPickerDialogView()->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kOk));

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
  test_api_.FocusSourceAtIndex(0);

  EXPECT_TRUE(GetPickerDialogView()->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kOk));

  GetPickerDialogView()->AcceptDialog();
  EXPECT_EQ(kFakeId, WaitForPickerResult());
}

// Regression test for https://crbug.com/1102153
TEST_P(DesktopMediaPickerViewsTest, DoneCallbackNotCalledOnDoubleTap) {
  const DesktopMediaID kFakeId(DesktopMediaID::TYPE_SCREEN, 222);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  if (test_api_.AudioSupported(DesktopMediaList::Type::kScreen)) {
    test_api_.SetAudioSharingApprovedByUser(false);
  }

  media_lists_[DesktopMediaList::Type::kScreen]->AddSourceByFullMediaID(
      kFakeId);
  test_api_.DoubleTapSourceAtIndex(0);
  EXPECT_FALSE(has_picker_result());
}

TEST_P(DesktopMediaPickerViewsTest, CancelButtonAlwaysEnabled) {
  EXPECT_TRUE(GetPickerDialogView()->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kCancel));
}

TEST_P(DesktopMediaPickerViewsTest, AudioCheckboxDefaultStates) {
  if (test_api_.AudioSupported(DesktopMediaList::Type::kScreen)) {
    test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
    EXPECT_FALSE(test_api_.IsAudioSharingApprovedByUser());
  }

  if (test_api_.AudioSupported(DesktopMediaList::Type::kWindow)) {
    test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
    EXPECT_FALSE(test_api_.IsAudioSharingApprovedByUser());
  }

  if (test_api_.AudioSupported(DesktopMediaList::Type::kWebContents)) {
    test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
    EXPECT_TRUE(test_api_.IsAudioSharingApprovedByUser());
  }
}

TEST_P(DesktopMediaPickerViewsTest, DistinctAudioCheckboxesHaveDistinctState) {
  DesktopMediaList::Type source_1 = DesktopMediaList::Type::kWebContents;
  DesktopMediaList::Type source_2 = DesktopMediaList::Type::kScreen;

  DCHECK(test_api_.AudioSupported(source_1));
  if (!test_api_.AudioSupported(source_2)) {
    return;  // Cannot run this particular variant of the test on this platform.
  }

  // Record source_1's audio state.
  test_api_.SelectTabForSourceType(source_1);
  const bool init_source_1_state = test_api_.IsAudioSharingApprovedByUser();

  // Toggle the audio state of source_2.
  test_api_.SelectTabForSourceType(source_2);
  const bool init_source_2_state = test_api_.IsAudioSharingApprovedByUser();
  const bool source_2_state = !init_source_2_state;
  test_api_.SetAudioSharingApprovedByUser(source_2_state);
  ASSERT_EQ(test_api_.IsAudioSharingApprovedByUser(), source_2_state);

  // The audio state of source_1 should remain unaffected.
  test_api_.SelectTabForSourceType(source_1);
  ASSERT_EQ(test_api_.IsAudioSharingApprovedByUser(), init_source_1_state);
}

// Verifies the visible status of audio checkbox.
// This test takes it as an article of faith that no checkbox is visible
// when GetAudioShareCheckbox() returns false.
TEST_P(DesktopMediaPickerViewsTest, AudioCheckboxVisibility) {
  bool is_system_audio_capture_supported =
      DesktopMediaPickerController::IsSystemAudioCaptureSupported(
          DesktopMediaPicker::Params::RequestSource::kGetDisplayMedia);
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  EXPECT_EQ(is_system_audio_capture_supported,
            test_api_.HasAudioShareControl());
  if (!is_system_audio_capture_supported) {
    EXPECT_EQ(test_api_.GetAudioLabelText(),
              l10n_util::GetStringUTF16(
                  IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB));
  }

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
  EXPECT_FALSE(test_api_.HasAudioShareControl());
  EXPECT_EQ(test_api_.GetAudioLabelText(),
            l10n_util::GetStringUTF16(
                is_system_audio_capture_supported
                    ? IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB_OR_SCREEN
                    : IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB));

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  EXPECT_TRUE(test_api_.HasAudioShareControl());
}

// Verifies that audio share information is recorded in the ID if the checkbox
// is checked.
TEST_P(DesktopMediaPickerViewsTest, DoneWithAudioShare) {
  constexpr DesktopMediaID kOriginId(DesktopMediaID::TYPE_WEB_CONTENTS, 222);

  DesktopMediaID result_id(DesktopMediaID::TYPE_WEB_CONTENTS, 222, true);

  // This matches the real workflow that when a source is generated in
  // media_list, its |audio_share| bit is not set. The bit is set by the picker
  // UI if the audio checkbox is checked.
  media_lists_[DesktopMediaList::Type::kWebContents]->AddSourceByFullMediaID(
      kOriginId);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  test_api_.SetAudioSharingApprovedByUser(true);
  test_api_.FocusSourceAtIndex(0);

  GetPickerDialogView()->AcceptDialog();
  EXPECT_EQ(result_id, WaitForPickerResult());
}

TEST_P(DesktopMediaPickerViewsTest, OkButtonEnabledDuringAcceptSpecific) {
  // The first pane is |tabs| in the new order and |screens| otherwise.
  DesktopMediaID::Type type = NewOrder() ? DesktopMediaID::TYPE_WEB_CONTENTS
                                         : DesktopMediaID::TYPE_SCREEN;
  DesktopMediaID fake_id(type, 222);

  media_lists_[DesktopMediaList::Type::kWindow]->AddSourceByFullMediaID(
      fake_id);
  if (NewOrder()) {
    // Audio-sharing supported for tabs, but not for windows.
    fake_id.web_contents_id.disable_local_echo = true;
    fake_id.audio_share = true;
  }

  EXPECT_FALSE(GetPickerDialogView()->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kOk));

  GetPickerDialogView()->AcceptSpecificSource(fake_id);
  EXPECT_EQ(fake_id, WaitForPickerResult());
}

#if BUILDFLAG(IS_MAC)
TEST_P(DesktopMediaPickerViewsTest, OnPermissionUpdateWithPermissions) {
  if (base::mac::MacOSMajorVersion() < 13) {
    GTEST_SKIP()
        << "ScreenCapturePermissionChecker only created for MacOS 13 and later";
  }

  test_api_.OnPermissionUpdate(true);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  EXPECT_TRUE(test_api_.GetActivePane()->IsContentPaneVisible());
  EXPECT_FALSE(test_api_.GetActivePane()->IsPermissionPaneVisible());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
  EXPECT_TRUE(test_api_.GetActivePane()->IsContentPaneVisible());
  EXPECT_FALSE(test_api_.GetActivePane()->IsPermissionPaneVisible());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  EXPECT_TRUE(test_api_.GetActivePane()->IsContentPaneVisible());
  EXPECT_FALSE(test_api_.GetActivePane()->IsPermissionPaneVisible());
}

TEST_P(DesktopMediaPickerViewsTest, OnPermissionUpdateWithoutPermissions) {
  if (base::mac::MacOSMajorVersion() < 13) {
    GTEST_SKIP()
        << "ScreenCapturePermissionChecker only created for MacOS 13 and later";
  }

  test_api_.OnPermissionUpdate(false);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  EXPECT_FALSE(test_api_.GetActivePane()->IsContentPaneVisible());
  EXPECT_TRUE(test_api_.GetActivePane()->IsPermissionPaneVisible());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
  EXPECT_FALSE(test_api_.GetActivePane()->IsContentPaneVisible());
  EXPECT_TRUE(test_api_.GetActivePane()->IsPermissionPaneVisible());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  EXPECT_TRUE(test_api_.GetActivePane()->IsContentPaneVisible());
  EXPECT_FALSE(test_api_.GetActivePane()->IsPermissionPaneVisible());
}
#endif

class DesktopMediaPickerViewsPerTypeTest
    : public DesktopMediaPickerViewsTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, DesktopMediaList::Type>> {
 public:
  DesktopMediaPickerViewsPerTypeTest()
      : DesktopMediaPickerViewsTestBase(GetSourceTypes(NewOrder())),
        source_id_type_(AsDesktopMediaIdType(type())) {}
  ~DesktopMediaPickerViewsPerTypeTest() override = default;

  void SetUp() override {
    // We must first call the base class SetUp, as if we skip without doing so,
    // then teardown will fail.
    DesktopMediaPickerViewsTestBase::SetUp();

    if (!base::Contains(source_types(), type())) {
      GTEST_SKIP();
    }

    test_api_.SelectTabForSourceType(type());
  }

  bool NewOrder() const { return std::get<0>(GetParam()); }
  DesktopMediaList::Type type() const { return std::get<1>(GetParam()); }
  DesktopMediaID::Type source_id_type() const { return source_id_type_; }

  static std::string GetDescription(
      const testing::TestParamInfo<
          DesktopMediaPickerViewsPerTypeTest::ParamType>& info) {
    const bool new_order = std::get<0>(info.param);
    const DesktopMediaList::Type type = std::get<1>(info.param);

    return base::StrCat(
        {new_order ? "New" : "Old", "Order", GetTypeAsTestNameString(type)});
  }

 private:
  const DesktopMediaID::Type source_id_type_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    DesktopMediaPickerViewsPerTypeTest,
    testing::Combine(/*NewOrder=*/testing::Bool(),
                     testing::Values(DesktopMediaList::Type::kWebContents,
                                     DesktopMediaList::Type::kWindow,
                                     DesktopMediaList::Type::kScreen)),
    &DesktopMediaPickerViewsPerTypeTest::GetDescription);

// Verifies that a MediaSourceView is selected with mouse left click and
// original selected MediaSourceView gets unselected.
TEST_P(DesktopMediaPickerViewsPerTypeTest, SelectMediaSourceViewOnSingleClick) {
  media_lists_[type()]->AddSourceByFullMediaID(
      DesktopMediaID(source_id_type(), 10));
  media_lists_[type()]->AddSourceByFullMediaID(
      DesktopMediaID(source_id_type(), 20));

  // By default, nothing should be selected.
  EXPECT_FALSE(test_api_.GetSelectedSourceId().has_value());

  test_api_.PressMouseOnSourceAtIndex(0);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());

  test_api_.PressMouseOnSourceAtIndex(1);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(20, test_api_.GetSelectedSourceId().value());
}

// Verifies that the MediaSourceView is added or removed when |media_list_| is
// updated.
TEST_P(DesktopMediaPickerViewsPerTypeTest, AddAndRemoveMediaSource) {
  // No media source at first.
  EXPECT_FALSE(test_api_.HasSourceAtIndex(0));

  for (int i = 0; i < 3; ++i) {
    media_lists_[type()]->AddSourceByFullMediaID(
        DesktopMediaID(source_id_type(), i));
    EXPECT_TRUE(test_api_.HasSourceAtIndex(i));
  }

  for (int i = 2; i >= 0; --i) {
    media_lists_[type()]->RemoveSource(i);
    EXPECT_FALSE(test_api_.HasSourceAtIndex(i));
  }
}

// Verifies that focusing the MediaSourceView marks it selected and the
// original selected MediaSourceView gets unselected.
TEST_P(DesktopMediaPickerViewsPerTypeTest, FocusMediaSourceViewToSelect) {
  media_lists_[type()]->AddSourceByFullMediaID(
      DesktopMediaID(source_id_type(), 10));
  media_lists_[type()]->AddSourceByFullMediaID(
      DesktopMediaID(source_id_type(), 20));

  test_api_.FocusSourceAtIndex(0);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());

  if (test_api_.AudioSupported(type())) {
    test_api_.FocusAudioShareControl();
    ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
    EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());
  }

  test_api_.FocusSourceAtIndex(1);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(20, test_api_.GetSelectedSourceId().value());
}

TEST_P(DesktopMediaPickerViewsPerTypeTest, OkButtonDisabledWhenNoSelection) {
  media_lists_[type()]->AddSourceByFullMediaID(
      DesktopMediaID(source_id_type(), 111));
  EXPECT_FALSE(GetPickerDialogView()->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kOk));

  test_api_.FocusSourceAtIndex(0);
  EXPECT_TRUE(GetPickerDialogView()->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kOk));

  media_lists_[type()]->RemoveSource(0);
  EXPECT_FALSE(GetPickerDialogView()->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kOk));
}

// Verifies that the controller can successfully clear the selection when asked
// to do so.
TEST_P(DesktopMediaPickerViewsPerTypeTest, ClearSelection) {
  media_lists_[type()]->AddSourceByFullMediaID(
      DesktopMediaID(source_id_type(), 10));
  media_lists_[type()]->AddSourceByFullMediaID(
      DesktopMediaID(source_id_type(), 20));

  // By default, nothing should be selected.
  EXPECT_FALSE(test_api_.GetSelectedSourceId().has_value());

  // Select a Source ID.
  test_api_.PressMouseOnSourceAtIndex(0);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());

  // Clear the selection and assert that nothing is selected.
  test_api_.GetSelectedController()->ClearSelection();
  EXPECT_FALSE(test_api_.GetSelectedSourceId().has_value());
}

#if BUILDFLAG(IS_WIN)
// When doing windows captures, Chromium is not allowed to share its own audio
// via application audio capture. This test suite asserts that the media picker
// UI is changed as expected when a Chromium window is selected - i.e., the
// audio-checkbox is disabled and unchecked.
class DesktopMediaPickerViewsApplicationAudioSharingControlTest
    : public DesktopMediaPickerViewsTestBase {
 public:
  DesktopMediaPickerViewsApplicationAudioSharingControlTest()
      : DesktopMediaPickerViewsTestBase(GetSourceTypes(/*new_order=*/true)) {}
  ~DesktopMediaPickerViewsApplicationAudioSharingControlTest() override =
      default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kApplicationAudioCaptureWin);
    DesktopMediaPickerViewsTestBase::SetUp();

    test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
    media_lists_[DesktopMediaList::Type::kWindow]->AddChromiumWindowSource(10);
    media_lists_[DesktopMediaList::Type::kWindow]->AddChromiumWindowSource(20);
    media_lists_[DesktopMediaList::Type::kWindow]->AddSource(30);
    media_lists_[DesktopMediaList::Type::kWindow]->AddSource(40);
  }

  void MaybeCreatePickerViews() override {
    CreatePickerViews(/*request_audio=*/true,
                      /*screen_exclude_system_audio=*/false,
                      blink::mojom::WindowAudioPreference::kWindow);
  }

  void CheckDefaultState() {
    // By default, nothing should be selected.
    EXPECT_FALSE(test_api_.GetSelectedSourceId().has_value());

    EXPECT_TRUE(test_api_.HasAudioShareControl());
    EXPECT_TRUE(test_api_.IsAudioSharingControlEnabled());
    EXPECT_TRUE(test_api_.IsAudioSharingApprovedByUser());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DesktopMediaPickerViewsApplicationAudioSharingControlTest,
       DefaultState) {
  CheckDefaultState();
}

TEST_F(DesktopMediaPickerViewsApplicationAudioSharingControlTest,
       ChromiumWindow) {
  CheckDefaultState();

  // Select first Chromium window.
  test_api_.FocusSourceAtIndex(0);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());
  EXPECT_TRUE(test_api_.HasAudioShareControl());
  EXPECT_FALSE(test_api_.IsAudioSharingControlEnabled());
  EXPECT_FALSE(test_api_.IsAudioSharingApprovedByUser());

  // Select second Chromium window.
  test_api_.FocusSourceAtIndex(1);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(20, test_api_.GetSelectedSourceId().value());
  EXPECT_TRUE(test_api_.HasAudioShareControl());
  EXPECT_FALSE(test_api_.IsAudioSharingControlEnabled());
  EXPECT_FALSE(test_api_.IsAudioSharingApprovedByUser());
}

TEST_F(DesktopMediaPickerViewsApplicationAudioSharingControlTest,
       NonChromiumWindow) {
  CheckDefaultState();

  // Select first non-Chromium window.
  test_api_.FocusSourceAtIndex(2);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(30, test_api_.GetSelectedSourceId().value());
  EXPECT_TRUE(test_api_.HasAudioShareControl());
  EXPECT_TRUE(test_api_.IsAudioSharingControlEnabled());
  EXPECT_TRUE(test_api_.IsAudioSharingApprovedByUser());

  // Select second non-Chromium window.
  test_api_.FocusSourceAtIndex(3);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(40, test_api_.GetSelectedSourceId().value());
  EXPECT_TRUE(test_api_.HasAudioShareControl());
  EXPECT_TRUE(test_api_.IsAudioSharingControlEnabled());
  EXPECT_TRUE(test_api_.IsAudioSharingApprovedByUser());
}

TEST_F(DesktopMediaPickerViewsApplicationAudioSharingControlTest,
       NonChromiumWindowStateIsPersisted) {
  CheckDefaultState();

  // Select first non-Chromium window.
  test_api_.FocusSourceAtIndex(2);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(30, test_api_.GetSelectedSourceId().value());
  EXPECT_TRUE(test_api_.HasAudioShareControl());
  EXPECT_TRUE(test_api_.IsAudioSharingControlEnabled());
  EXPECT_TRUE(test_api_.IsAudioSharingApprovedByUser());

  // Select first Chromium window.
  test_api_.FocusSourceAtIndex(0);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());
  EXPECT_TRUE(test_api_.HasAudioShareControl());
  EXPECT_FALSE(test_api_.IsAudioSharingControlEnabled());
  EXPECT_FALSE(test_api_.IsAudioSharingApprovedByUser());

  // Re-select second non-Chromium window and verify that audio sharing state is
  // persisted.
  test_api_.FocusSourceAtIndex(3);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(40, test_api_.GetSelectedSourceId().value());
  EXPECT_TRUE(test_api_.HasAudioShareControl());
  EXPECT_TRUE(test_api_.IsAudioSharingControlEnabled());
  EXPECT_TRUE(test_api_.IsAudioSharingApprovedByUser());

  // Uncheck audio sharing.
  test_api_.SetAudioSharingApprovedByUser(false);
  EXPECT_FALSE(test_api_.IsAudioSharingApprovedByUser());

  // Select first Chromium window again.
  test_api_.FocusSourceAtIndex(0);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());
  EXPECT_TRUE(test_api_.HasAudioShareControl());
  EXPECT_FALSE(test_api_.IsAudioSharingControlEnabled());
  EXPECT_FALSE(test_api_.IsAudioSharingApprovedByUser());

  // Re-select first non-Chromium window and verify that audio sharing state is
  // persisted.
  test_api_.FocusSourceAtIndex(2);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(30, test_api_.GetSelectedSourceId().value());
  EXPECT_TRUE(test_api_.HasAudioShareControl());
  EXPECT_TRUE(test_api_.IsAudioSharingControlEnabled());
  EXPECT_FALSE(test_api_.IsAudioSharingApprovedByUser());
}

#endif  // BUILDFLAG(IS_WIN)

class DesktopMediaPickerViewsPerTypeAndAudioTest
    : public DesktopMediaPickerViewsTestBase,
      public testing::WithParamInterface<std::tuple<
          DesktopMediaList::Type,
          /*RequireAudio=*/bool,
          /*SystemAudio=*/bool,
          /*WindowAudioPreference=*/blink::mojom::WindowAudioPreference>> {
 public:
  DesktopMediaPickerViewsPerTypeAndAudioTest()
      : DesktopMediaPickerViewsTestBase(GetSourceTypes(/*new_order=*/true)) {}
  ~DesktopMediaPickerViewsPerTypeAndAudioTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    feature_list_.InitAndEnableFeature(features::kApplicationAudioCaptureWin);
#endif  // BUILDFLAG(IS_WIN)
    DesktopMediaPickerViewsTestBase::SetUp();
  }

  void MaybeCreatePickerViews() override {
    CreatePickerViews(RequireAudio(), SystemAudio(), WindowAudioPreference());
  }

  DesktopMediaList::Type Type() const { return std::get<0>(GetParam()); }
  bool RequireAudio() const { return std::get<1>(GetParam()); }
  bool SystemAudio() const { return std::get<2>(GetParam()); }
  blink::mojom::WindowAudioPreference WindowAudioPreference() const {
    return std::get<3>(GetParam());
  }

 private:
#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList feature_list_;
#endif  // BUILDFLAG(IS_WIN)
};

INSTANTIATE_TEST_SUITE_P(
    ,
    DesktopMediaPickerViewsPerTypeAndAudioTest,
    testing::Combine(
        testing::Values(DesktopMediaList::Type::kWebContents,
                        DesktopMediaList::Type::kWindow,
                        DesktopMediaList::Type::kScreen),
        testing::Bool(),
        testing::Bool(),
        testing::Values(blink::mojom::WindowAudioPreference::kExclude,
                        blink::mojom::WindowAudioPreference::kSystem,
                        blink::mojom::WindowAudioPreference::kWindow)));

TEST_P(DesktopMediaPickerViewsPerTypeAndAudioTest, AcceptSpecific) {
  DesktopMediaID fake_id(AsDesktopMediaIdType(Type()), 333);

  // Audio is enabled by default for tabs
  if (RequireAudio() &&
      AsDesktopMediaIdType(Type()) == DesktopMediaID::TYPE_WEB_CONTENTS) {
    fake_id.audio_share = true;
  } else if (AsDesktopMediaIdType(Type()) == DesktopMediaID::TYPE_WINDOW) {
    // For Window captures, the `window_audio_type` will be set based on
    // platform support.
    fake_id.window_audio_type = test_api_.GetWindowAudioType();

    // Audio is enabled by default for window application audio capture.
    fake_id.audio_share =
        RequireAudio() && test_api_.GetWindowAudioType() ==
                              DesktopMediaID::AudioType::kApplication;
  }
  media_lists_[Type()]->AddSourceByFullMediaID(fake_id);

  GetPickerDialogView()->AcceptSpecificSource(fake_id);
  EXPECT_EQ(fake_id, WaitForPickerResult());
}

class DesktopMediaPickerViewsSystemAudioTest
    : public DesktopMediaPickerViewsTestBase {
 public:
  DesktopMediaPickerViewsSystemAudioTest()
      : DesktopMediaPickerViewsTestBase(GetSourceTypes(/*new_order=*/false)) {}
  ~DesktopMediaPickerViewsSystemAudioTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_MAC)
    feature_list_.InitWithFeatures(
        {media::kMacCatapLoopbackAudioForCast,
         media::kMacCatapLoopbackAudioForScreenShare},
        {});
#endif
    DesktopMediaPickerViewsTestBase::SetUp();
  }

  void MaybeCreatePickerViews() override {
    // CreatePickerViews() called  directly from tests.
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DesktopMediaPickerViewsSystemAudioTest,
       SystemAudioCheckboxVisibleIfExcludeSystemAudioNotSpecified) {
  CreatePickerViews(/*request_audio=*/true, /*exclude_system_audio=*/false,
                    blink::mojom::WindowAudioPreference::kSystem);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);

  // System audio checkbox shown to the user iff the platform supports it.
  EXPECT_EQ(DesktopMediaPickerController::IsSystemAudioCaptureSupported(
                DesktopMediaPicker::Params::RequestSource::kGetDisplayMedia),
            test_api_.HasAudioShareControl());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);

  // System audio checkbox shown to the user iff the platform supports it.
  EXPECT_EQ(DesktopMediaPickerController::IsSystemAudioCaptureSupported(
                DesktopMediaPicker::Params::RequestSource::kGetDisplayMedia),
            test_api_.HasAudioShareControl());
}

TEST_F(DesktopMediaPickerViewsSystemAudioTest,
       SystemAudioCheckboxInvisibleIfExcludeSystemAudioSpecified) {
  CreatePickerViews(/*request_audio=*/true, /*exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);

  // Main expectation: System audio control not shown to the user, only a hint
  // to select a tab instead.
  EXPECT_FALSE(test_api_.HasAudioShareControl());
  EXPECT_EQ(
      test_api_.GetAudioLabelText(),
      l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB));

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);

  // Main expectation: System audio control not shown to the user, only a hint
  // to select a tab instead.
  EXPECT_FALSE(test_api_.HasAudioShareControl());
  EXPECT_EQ(
      test_api_.GetAudioLabelText(),
      l10n_util::GetStringUTF16(IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB));

  // Secondary expectation: No effect on the tab-audio checkbox.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  EXPECT_TRUE(test_api_.HasAudioShareControl());
}

TEST_F(DesktopMediaPickerViewsSystemAudioTest,
       IfAudioNotRequestedThenExcludeSystemAudioHasNoEffect) {
  CreatePickerViews(/*request_audio=*/false, /*exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);

  // Main expectation: System audio checkbox not shown to the user.
  EXPECT_FALSE(test_api_.HasAudioShareControl());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);

  // Main expectation: System audio checkbox not shown to the user.
  EXPECT_FALSE(test_api_.HasAudioShareControl());

  // Secondary expectation: No effect on the tab-audio checkbox.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  EXPECT_FALSE(test_api_.HasAudioShareControl());  // Not requested.
}

TEST_F(DesktopMediaPickerViewsSystemAudioTest,
       CorrectHintsIfSystemAudioIsExcluded) {
  CreatePickerViews(/*request_audio=*/true, /*exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kSystem);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);

  // Main expectation: System audio checkbox not shown to the user.
  EXPECT_FALSE(test_api_.HasAudioShareControl());

  EXPECT_EQ(test_api_.GetAudioLabelText(),
            l10n_util::GetStringUTF16(
                DesktopMediaPickerController::IsSystemAudioCaptureSupported(
                    DesktopMediaPicker::Params::RequestSource::kGetDisplayMedia)
                    ? IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB_OR_WINDOW
                    : IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB));
}

TEST_F(DesktopMediaPickerViewsSystemAudioTest,
       CorrectHintsIfWindowAudioIsExcluded) {
  CreatePickerViews(/*request_audio=*/true, /*exclude_system_audio=*/false,
                    blink::mojom::WindowAudioPreference::kExclude);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);

  // Main expectation: System audio checkbox not shown to the user.
  EXPECT_FALSE(test_api_.HasAudioShareControl());

  EXPECT_EQ(test_api_.GetAudioLabelText(),
            l10n_util::GetStringUTF16(
                DesktopMediaPickerController::IsSystemAudioCaptureSupported(
                    DesktopMediaPicker::Params::RequestSource::kGetDisplayMedia)
                    ? IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB_OR_SCREEN
                    : IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB));
}

#if BUILDFLAG(IS_WIN)
// Verifies the conditions that make the media picker audio checkbox enabled for
// each type of pane (Tab, Window, Screen) when application audio capture is
// available/unavailable. Also checks that the checkbox string is correct for
// each type of pane. Application audio capture is currently only supported on
// Windows.
class DesktopMediaPickerViewsApplicationAudioTest
    : public DesktopMediaPickerViewsTestBase,
      public testing::WithParamInterface<
          std::tuple<blink::mojom::WindowAudioPreference,
                     /*is_application_audio_capture_supported=*/bool,
                     /*request_audio=*/bool,
                     /*screen_exclude_system_audio=*/bool>> {
 public:
  DesktopMediaPickerViewsApplicationAudioTest()
      : DesktopMediaPickerViewsTestBase(GetSourceTypes(/*new_order=*/false)) {}
  ~DesktopMediaPickerViewsApplicationAudioTest() override = default;

  void SetUp() override {
    if (ShouldEnableApplicationAudioCapture()) {
      feature_list_.InitAndEnableFeature(features::kApplicationAudioCaptureWin);
    } else {
      feature_list_.InitAndDisableFeature(
          features::kApplicationAudioCaptureWin);
    }
    DesktopMediaPickerViewsTestBase::SetUp();
  }

  void MaybeCreatePickerViews() override {
    // CreatePickerViews() called  directly from tests.
  }

  blink::mojom::WindowAudioPreference WindowAudioPreference() const {
    return std::get<0>(GetParam());
  }

  bool ShouldEnableApplicationAudioCapture() const {
    return std::get<1>(GetParam());
  }

  bool RequestAudio() const { return std::get<2>(GetParam()); }

  // Returns true if the `systemAudio` parameter passed to getisplayMedia was
  // set to "exclude".
  bool ScreenExcludeSystemAudio() const { return std::get<3>(GetParam()); }

  // Returns true if the screen tab should offer audio sharing.
  bool ShouldOfferScreenAudio() const {
    return RequestAudio() && !ScreenExcludeSystemAudio();
  }

  // Returns true if the window tab should offer audio sharing.
  bool ShouldOfferWindowAudio() const {
    return RequestAudio() &&
           WindowAudioPreference() !=
               blink::mojom::WindowAudioPreference::kExclude &&
           (((media::IsApplicationAudioCaptureSupported() &&
              WindowAudioPreference() ==
                  blink::mojom::WindowAudioPreference::kWindow)) ||
            (WindowAudioPreference() ==
             blink::mojom::WindowAudioPreference::kSystem));
  }

  // Returns the expected label for the screen pane's audio toggle of the
  // getDisplayMedia picker UI.
  std::u16string GetExpectedScreenAudioLabel() const {
    if (!RequestAudio()) {
      return std::u16string();
    }

    if (ShouldOfferScreenAudio()) {
      return l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_ALSO_SHARE_SYSTEM_AUDIO);
    }

    if (ShouldOfferWindowAudio()) {
      return l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB_OR_WINDOW);
    }

    return l10n_util::GetStringUTF16(
        IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB);
  }

  // Returns the expected label for the window pane's audio toggle of the
  // getDisplayMedia picker UI.
  std::u16string GetExpectedWindowAudioLabel() const {
    if (!RequestAudio()) {
      return std::u16string();
    }

    if (ShouldOfferWindowAudio()) {
      if (WindowAudioPreference() ==
              blink::mojom::WindowAudioPreference::kWindow &&
          media::IsApplicationAudioCaptureSupported()) {
        return l10n_util::GetStringUTF16(
            IDS_DESKTOP_MEDIA_PICKER_ALSO_SHARE_APPLICATION_AUDIO);
      }

      return l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_ALSO_SHARE_SYSTEM_AUDIO);
    }

    if (ShouldOfferScreenAudio()) {
      return l10n_util::GetStringUTF16(
          IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB_OR_SCREEN);
    }

    return l10n_util::GetStringUTF16(
        IDS_DESKTOP_MEDIA_PICKER_AUDIO_SHARE_HINT_TAB);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    DesktopMediaPickerViewsApplicationAudioTest,
    testing::Combine(
        testing::Values(blink::mojom::WindowAudioPreference::kExclude,
                        blink::mojom::WindowAudioPreference::kSystem,
                        blink::mojom::WindowAudioPreference::kWindow),
        testing::Bool(),
        testing::Bool(),
        testing::Bool()));

TEST_P(DesktopMediaPickerViewsApplicationAudioTest, AudioCheckbox) {
  CreatePickerViews(RequestAudio(), ScreenExcludeSystemAudio(),
                    WindowAudioPreference());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  EXPECT_EQ(test_api_.HasAudioShareControl(), ShouldOfferScreenAudio());
  EXPECT_EQ(test_api_.IsScreenAudioOffered(), ShouldOfferScreenAudio());
  if (ShouldOfferScreenAudio()) {
    EXPECT_FALSE(test_api_.IsAudioSharingApprovedByUser());
  }
  EXPECT_EQ(test_api_.GetAudioLabelText(), GetExpectedScreenAudioLabel());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
  EXPECT_EQ(test_api_.HasAudioShareControl(), ShouldOfferWindowAudio());
  EXPECT_EQ(test_api_.IsWindowAudioOffered(), ShouldOfferWindowAudio());
  // By default, the audio sharing toggle is checked for application audio,
  // and unchecked for system audio.
  if (ShouldOfferWindowAudio()) {
    if (test_api_.GetWindowAudioType() ==
        DesktopMediaID::AudioType::kApplication) {
      EXPECT_TRUE(test_api_.IsAudioSharingApprovedByUser());
    } else {
      EXPECT_FALSE(test_api_.IsAudioSharingApprovedByUser());
    }
  }
  EXPECT_EQ(test_api_.GetAudioLabelText(), GetExpectedWindowAudioLabel());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  EXPECT_EQ(test_api_.HasAudioShareControl(), RequestAudio());
}
#endif  // BUILDFLAG(IS_WIN)

// Creates a single pane DesktopMediaPickerImpl that only has a tab list.
class DesktopMediaPickerViewsSingleTabPaneTest
    : public DesktopMediaPickerViewsTestBase {
 public:
  DesktopMediaPickerViewsSingleTabPaneTest()
      : DesktopMediaPickerViewsTestBase(
            {DesktopMediaList::Type::kWebContents}) {}
  ~DesktopMediaPickerViewsSingleTabPaneTest() override = default;

 protected:
  void AddTabSource() {
    media_lists_[DesktopMediaList::Type::kWebContents]->AddSourceByFullMediaID(
        DesktopMediaID(DesktopMediaID::TYPE_WEB_CONTENTS, 0));
  }
};

// Validates that the tab list's preferred size is not zero.
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
  for (int i = 0; i < 9; i++) {
    AddTabSource();
  }
  EXPECT_EQ(GetDialogHeight(), initial_size);

  // The dialog's height should be fixed and equal to the equivalent of ten
  // rows, thus it should not change when going from nine to eleven either.
  AddTabSource();
  EXPECT_EQ(GetDialogHeight(), initial_size);
  AddTabSource();
  EXPECT_EQ(GetDialogHeight(), initial_size);

  // And then it shouldn't change when going to a larger number of sources.
  for (int i = 0; i < 50; i++) {
    AddTabSource();
  }
  EXPECT_EQ(GetDialogHeight(), initial_size);

  // And then it shouldn't change when going from a large number of sources (in
  // this case 61) to a larger number, because the ScrollView should scroll
  // large numbers of sources.
  for (int i = 0; i < 50; i++) {
    AddTabSource();
  }
  EXPECT_EQ(GetDialogHeight(), initial_size);
}

// Regression test for https://crbug.com/1042976.
TEST_F(DesktopMediaPickerViewsSingleTabPaneTest,
       CannotAcceptTabWithoutSelection) {
  AddTabSource();
  AddTabSource();
  AddTabSource();

  test_api_.FocusSourceAtIndex(0, false);
  EXPECT_EQ(std::nullopt, test_api_.GetSelectedSourceId());
  EXPECT_FALSE(GetPickerDialogView()->IsDialogButtonEnabled(
      ui::mojom::DialogButton::kOk));

  // Send the tab list a Return key press, to make sure it doesn't try to accept
  // with no selected source. If the fix to https://crbug.com/1042976 regresses,
  // this test will crash here.
  test_api_.PressKeyOnSourceAtIndex(
      0, ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RETURN, 0));
}

// Tests accessible properties of DesktopMediaListView and
// DesktopMediaSourceView.
TEST_F(DesktopMediaPickerViewsSingleTabPaneTest, AccessibleProperties) {
  ui::AXNodeData data;
  DesktopMediaSourceViewStyle style = DesktopMediaSourceViewStyle(
      /*columns=*/2,
      /*item_size=*/gfx::Size(266, 224),
      /*icon_rect=*/gfx::Rect(),
      /*label_rect=*/gfx::Rect(8, 196, 250, 36),
      /*text_alignment=*/gfx::HorizontalAlignment::ALIGN_CENTER,
      /*image_rect=*/gfx::Rect(8, 8, 250, 180));

  // DesktopMediaListView accessible properties test.

  std::u16string sample_accessible_name = u"Sample accessible name";
  auto list_view = std::make_unique<DesktopMediaListView>(
      test_api_.GetSelectedController(), style, style, sample_accessible_name);

  list_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kGroup);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            sample_accessible_name);
  EXPECT_EQ(list_view->GetViewAccessibility().GetCachedName(),
            sample_accessible_name);

  // DesktopMediaSourceView accessible properties test.
  const content::DesktopMediaID media_id(content::DesktopMediaID::TYPE_SCREEN,
                                         content::DesktopMediaID::kFakeId);
  auto source_view = std::make_unique<DesktopMediaSourceView>(list_view.get(),
                                                              media_id, style);
  data = ui::AXNodeData();

  ASSERT_TRUE(source_view);
  source_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(
                IDS_DESKTOP_MEDIA_SOURCE_EMPTY_ACCESSIBLE_NAME));
  EXPECT_EQ(source_view->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(
                IDS_DESKTOP_MEDIA_SOURCE_EMPTY_ACCESSIBLE_NAME));

  source_view->SetName(sample_accessible_name);

  data = ui::AXNodeData();
  source_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            sample_accessible_name);
  EXPECT_EQ(source_view->GetViewAccessibility().GetCachedName(),
            sample_accessible_name);

  // DesktopMediaDelegatedSourceListView accessible properties test.
  auto view = std::make_unique<DesktopMediaDelegatedSourceListView>(
      test_api_.GetSelectedController()->GetWeakPtr(), sample_accessible_name,
      DesktopMediaList::Type::kScreen);

  data = ui::AXNodeData();
  view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kGroup);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            sample_accessible_name);
}

class DesktopMediaPickerPreferredDisplaySurfaceTest
    : public DesktopMediaPickerViewsTestBase,
      public testing::WithParamInterface<
          std::tuple<bool, blink::mojom::PreferredDisplaySurface>> {
 public:
  DesktopMediaPickerPreferredDisplaySurfaceTest()
      : DesktopMediaPickerViewsTestBase(GetSourceTypes(NewOrder())) {}

  void MaybeCreatePickerViews() override {
    CreatePickerViews(/*request_audio=*/true,
                      /*screen_exclude_system_audio=*/false,
                      blink::mojom::WindowAudioPreference::kExclude,
                      PreferredDisplaySurface());
  }

  bool NewOrder() const { return std::get<0>(GetParam()); }

  blink::mojom::PreferredDisplaySurface PreferredDisplaySurface() const {
    return std::get<1>(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    DesktopMediaPickerPreferredDisplaySurfaceTest,
    testing::Combine(
        /*NewOrder=*/testing::Bool(),
        testing::Values(blink::mojom::PreferredDisplaySurface::NO_PREFERENCE,
                        blink::mojom::PreferredDisplaySurface::MONITOR,
                        blink::mojom::PreferredDisplaySurface::WINDOW,
                        blink::mojom::PreferredDisplaySurface::BROWSER)));

TEST_P(DesktopMediaPickerPreferredDisplaySurfaceTest,
       SelectedTabMatchesPreferredDisplaySurface) {
  switch (PreferredDisplaySurface()) {
    case blink::mojom::PreferredDisplaySurface::NO_PREFERENCE:
      EXPECT_EQ(test_api_.GetSelectedSourceListType(),
                NewOrder() ? DesktopMediaList::Type::kWebContents
                           : DesktopMediaList::Type::kScreen);
      break;
    case blink::mojom::PreferredDisplaySurface::MONITOR:
      EXPECT_EQ(test_api_.GetSelectedSourceListType(),
                DesktopMediaList::Type::kScreen);
      break;
    case blink::mojom::PreferredDisplaySurface::WINDOW:
      EXPECT_EQ(test_api_.GetSelectedSourceListType(),
                DesktopMediaList::Type::kWindow);
      break;
    case blink::mojom::PreferredDisplaySurface::BROWSER:
      EXPECT_EQ(test_api_.GetSelectedSourceListType(),
                DesktopMediaList::Type::kWebContents);
      break;
  }
}

class DesktopMediaPickerDoubleClickTest
    : public DesktopMediaPickerViewsTestBase,
      public testing::WithParamInterface<
          std::pair<DesktopMediaList::Type, DesktopMediaID::Type>> {
 public:
  DesktopMediaPickerDoubleClickTest()
      : DesktopMediaPickerViewsTestBase(GetSourceTypes(/*new_order=*/true)) {}
};

INSTANTIATE_TEST_SUITE_P(
    ,
    DesktopMediaPickerDoubleClickTest,
    testing::Values(std::make_pair(DesktopMediaList::Type::kWindow,
                                   DesktopMediaID::TYPE_WINDOW),
                    std::make_pair(DesktopMediaList::Type::kScreen,
                                   DesktopMediaID::TYPE_SCREEN),
                    std::make_pair(DesktopMediaList::Type::kWebContents,
                                   DesktopMediaID::TYPE_WEB_CONTENTS)));

// Regression test for https://crbug.com/1102153 and https://crbug.com/1127496
TEST_P(DesktopMediaPickerDoubleClickTest, DoneCallbackNotCalledOnDoubleClick) {
  const DesktopMediaList::Type media_list_type = std::get<0>(GetParam());
  const DesktopMediaID::Type media_type = std::get<1>(GetParam());

  const DesktopMediaID kFakeId(media_type, 222);

  media_lists_[media_list_type]->AddSourceByFullMediaID(kFakeId);
  test_api_.SelectTabForSourceType(media_list_type);
  test_api_.PressMouseOnSourceAtIndex(0, true);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop_.QuitClosure());
  run_loop_.Run();

  EXPECT_FALSE(has_picker_result());
}

// This class expects tests to directly call first SetSourceTypes() and then
// CreatePickerViews().
class DelegatedSourceListTest : public DesktopMediaPickerViewsTestBase {
 public:
  DelegatedSourceListTest() : DesktopMediaPickerViewsTestBase({}) {}
  ~DelegatedSourceListTest() override = default;

  void MaybeCreatePickerViews() override {}

  void SetSourceTypes(
      const std::vector<DesktopMediaList::Type>& source_types,
      const std::vector<DesktopMediaList::Type>& delegated_source_types) {
    source_types_ = source_types;
    delegated_source_types_ = delegated_source_types;
    ASSERT_FALSE(
        std::ranges::any_of(source_types_, [this](DesktopMediaList::Type type) {
          return base::Contains(delegated_source_types_, type);
        }));
  }
};

// Ensures that Focus/Hide View events get plumbed correctly to the source lists
// upon the view being selected or not.
TEST_F(DelegatedSourceListTest, EnsureFocus) {
  SetSourceTypes(
      {DesktopMediaList::Type::kWebContents},
      {DesktopMediaList::Type::kScreen, DesktopMediaList::Type::kWindow});
  CreatePickerViews(/*request_audio=*/false,
                    /*screen_exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  EXPECT_FALSE(media_lists_[DesktopMediaList::Type::kScreen]->is_focused());
  EXPECT_FALSE(media_lists_[DesktopMediaList::Type::kWindow]->is_focused());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  EXPECT_TRUE(media_lists_[DesktopMediaList::Type::kScreen]->is_focused());
  EXPECT_FALSE(media_lists_[DesktopMediaList::Type::kWindow]->is_focused());

  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
  EXPECT_FALSE(media_lists_[DesktopMediaList::Type::kScreen]->is_focused());
  EXPECT_TRUE(media_lists_[DesktopMediaList::Type::kWindow]->is_focused());
}

#if BUILDFLAG(IS_MAC)

// Ensures that the first (only) source from a delegated source list is
// selected.
TEST_F(DelegatedSourceListTest, TestSelection) {
  SetSourceTypes(
      {DesktopMediaList::Type::kWebContents},
      {DesktopMediaList::Type::kScreen, DesktopMediaList::Type::kWindow});
  CreatePickerViews(/*request_audio=*/false,
                    /*screen_exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  // Add the one entry that is expected for a delegated source list and switch
  // to it. Note that since this is a delegated source, we must select its pane
  // before the observer will be set for adding items to the list.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  media_lists_[DesktopMediaList::Type::kScreen]->AddSourceByFullMediaID(
      DesktopMediaID(AsDesktopMediaIdType(DesktopMediaList::Type::kScreen),
                     10));

  // On MacOS, the added source is automatically selected.
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());
}

#else

// Ensures that the first (only) source from a delegated source list is selected
// after being notified that it has made a selection.
TEST_F(DelegatedSourceListTest, TestSelection) {
  SetSourceTypes(
      {DesktopMediaList::Type::kWebContents},
      {DesktopMediaList::Type::kScreen, DesktopMediaList::Type::kWindow});
  CreatePickerViews(/*request_audio=*/false,
                    /*screen_exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  // Add the one entry that is expected for a delegated source list and switch
  // to it. Note that since this is a delegated source, we must select its pane
  // before the observer will be set for adding items to the list.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  media_lists_[DesktopMediaList::Type::kScreen]->AddSourceByFullMediaID(
      DesktopMediaID(AsDesktopMediaIdType(DesktopMediaList::Type::kScreen),
                     10));
  ASSERT_FALSE(test_api_.GetSelectedSourceId().has_value());

  // Indicate that a selection has been made and ensure that our one source is
  // now selected.
  media_lists_[DesktopMediaList::Type::kScreen]
      ->OnDelegatedSourceListSelection();

  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());
}

#endif  // BUILDFLAG(IS_MACOS)

// Creates a single pane picker and verifies that when it gets notified that the
// delegated source list is dismissed that it finishes without a selection.
TEST_F(DelegatedSourceListTest, SinglePaneReject) {
  SetSourceTypes({}, {DesktopMediaList::Type::kScreen});
  CreatePickerViews(/*request_audio=*/false,
                    /*screen_exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  media_lists_[DesktopMediaList::Type::kScreen]
      ->OnDelegatedSourceListDismissed();

  EXPECT_EQ(
      WaitForPickerResult(),
      base::unexpected(MediaStreamRequestResult::PERMISSION_DENIED_BY_USER));
}

// Creates a picker without the default fallback pane and verifies that when it
// gets notified that the delegated source list is dismissed that it finishes
// without a selection.
TEST_F(DelegatedSourceListTest, NoFallbackPaneReject) {
  // kWebContents is the fallback type, so give two types but ensure that it
  // isn't one of them.
  SetSourceTypes(
      {}, {DesktopMediaList::Type::kScreen, DesktopMediaList::Type::kWindow});
  CreatePickerViews(/*request_audio=*/false,
                    /*screen_exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  media_lists_[DesktopMediaList::Type::kScreen]
      ->OnDelegatedSourceListDismissed();
  EXPECT_EQ(
      WaitForPickerResult(),
      base::unexpected(MediaStreamRequestResult::PERMISSION_DENIED_BY_USER));
}

#if BUILDFLAG(IS_MAC)
// Creates a picker with the default fallback pane and verifies that when it
// gets notified that the delegated source list is dismissed that it closes the
// picker.
TEST_F(DelegatedSourceListTest, ClosePickerOnSourceListDismissed) {
  // WebContents is the fallback type, so need to have a picker with it and
  // one other type.
  SetSourceTypes({DesktopMediaList::Type::kWebContents},
                 {DesktopMediaList::Type::kScreen});
  CreatePickerViews(/*request_audio=*/false,
                    /*screen_exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  // Switch to the screen pane and simulate the user dismissing the native
  // picker.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  media_lists_[DesktopMediaList::Type::kScreen]
      ->OnDelegatedSourceListDismissed();

  // Dismissing the delegated source list should close the picker
  // without a selection.
  EXPECT_EQ(
      WaitForPickerResult(),
      base::unexpected(MediaStreamRequestResult::PERMISSION_DENIED_BY_USER));
}

// The delegated picker experience on MacOS (using SCContentSharingPicker)
// starts the capture immediately after the user has made their choice, so
// the reselect button is not enabled for any capture type
TEST_F(DelegatedSourceListTest, ReselectButtonAbsent) {
  SetSourceTypes(
      {DesktopMediaList::Type::kWebContents},
      {DesktopMediaList::Type::kScreen, DesktopMediaList::Type::kWindow});
  CreatePickerViews(/*request_audio=*/false,
                    /*screen_exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  // Ensure that we don't have a reselect button for the non-delegated type.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  EXPECT_EQ(nullptr, test_api_.GetReselectButton());

  // Ensure that we don't have a reselect button for the screen delegated type.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  ASSERT_EQ(nullptr, test_api_.GetReselectButton());

  // Ensure that we don't have a reselect button for window delegated type.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
  ASSERT_EQ(nullptr, test_api_.GetReselectButton());
}

#else
// Creates a picker with the default fallback pane and verifies that when it
// gets notified that the delegated source list is dismissed that it switches
// to that pane.
TEST_F(DelegatedSourceListTest, SwitchToWebContents) {
  // WebContents is the fallback type, so need to have a picker with it and
  // one other type.
  SetSourceTypes({DesktopMediaList::Type::kWebContents},
                 {DesktopMediaList::Type::kScreen});
  CreatePickerViews(/*request_audio=*/false,
                    /*screen_exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  // Switch to the screen pane, dismiss it, then validate that we're back on
  // the WebContents pane.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  media_lists_[DesktopMediaList::Type::kScreen]
      ->OnDelegatedSourceListDismissed();

  EXPECT_EQ(DesktopMediaList::Type::kWebContents,
            test_api_.GetSelectedSourceListType());
}

// Creates a picker with the default fallback pane and verifies that when it
// gets notified that the delegated source list is dismissed that it switches
// to that pane and clears any previous selection.
TEST_F(DelegatedSourceListTest, EnsureNoWebContentsSelected) {
  // Ensure that we have the (Fallback) WebContents type and a different type
  SetSourceTypes({DesktopMediaList::Type::kWebContents},
                 {DesktopMediaList::Type::kScreen});
  CreatePickerViews(/*request_audio=*/false,
                    /*screen_exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);
  const auto web_contents_source_type =
      AsDesktopMediaIdType(DesktopMediaList::Type::kWebContents);

  // Add a couple of tabs
  media_lists_[DesktopMediaList::Type::kWebContents]->AddSourceByFullMediaID(
      DesktopMediaID(web_contents_source_type, 10));
  media_lists_[DesktopMediaList::Type::kWebContents]->AddSourceByFullMediaID(
      DesktopMediaID(web_contents_source_type, 20));

  // Select a tab
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  test_api_.PressMouseOnSourceAtIndex(0);
  ASSERT_TRUE(test_api_.GetSelectedSourceId().has_value());
  EXPECT_EQ(10, test_api_.GetSelectedSourceId().value());

  // Switch to screen and then indicate that we need to switch back because it
  // has been dismissed.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  media_lists_[DesktopMediaList::Type::kScreen]
      ->OnDelegatedSourceListDismissed();

  // We should be back on the tab pane with no item selected.
  EXPECT_EQ(DesktopMediaList::Type::kWebContents,
            test_api_.GetSelectedSourceListType());
  ASSERT_FALSE(test_api_.GetSelectedSourceId().has_value());
}

// Verify that the reselect button is only present on the delegated source list
// type panes.
TEST_F(DelegatedSourceListTest, ReselectButtonEnabled) {
  SetSourceTypes(
      {DesktopMediaList::Type::kWebContents},
      {DesktopMediaList::Type::kScreen, DesktopMediaList::Type::kWindow});
  CreatePickerViews(/*request_audio=*/false,
                    /*screen_exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  // Ensure that we don't have a reselect button for the non-delegated type.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  EXPECT_EQ(nullptr, test_api_.GetReselectButton());

  // Ensure that we do have a reselect button for the screen delegated type, and
  // that it is not enabled by default.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  ASSERT_NE(nullptr, test_api_.GetReselectButton());
  EXPECT_FALSE(test_api_.GetReselectButton()->GetEnabled());

  // Ensure that the reselect button is cleared by switching back to the non
  // delegated type.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWebContents);
  EXPECT_EQ(nullptr, test_api_.GetReselectButton());

  // Ensure that we do have a reselect button for the window delegated type, and
  // that it is not enabled by default.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
  ASSERT_NE(nullptr, test_api_.GetReselectButton());
  EXPECT_FALSE(test_api_.GetReselectButton()->GetEnabled());
}

// Verifies that the reselect button is disabled until a selection has been
// made in the delegated source list, and then disables itself again after a
// click.
TEST_F(DelegatedSourceListTest, ReselectButtonEnabledState) {
  SetSourceTypes(
      {DesktopMediaList::Type::kWebContents},
      {DesktopMediaList::Type::kScreen, DesktopMediaList::Type::kWindow});
  CreatePickerViews(/*request_audio=*/false,
                    /*screen_exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  // Ensure that we do have a reselect button for the screen delegated type, and
  // that it is not enabled by default.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  ASSERT_NE(nullptr, test_api_.GetReselectButton());
  EXPECT_FALSE(test_api_.GetReselectButton()->GetEnabled());

  // Simulate a selection and verify that the reselect button gets enabled.
  media_lists_[DesktopMediaList::Type::kScreen]
      ->OnDelegatedSourceListSelection();
  EXPECT_TRUE(test_api_.GetReselectButton()->GetEnabled());

  // Verify that the other Reselect button remains disabled.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kWindow);
  ASSERT_NE(nullptr, test_api_.GetReselectButton());
  EXPECT_FALSE(test_api_.GetReselectButton()->GetEnabled());

  // Verify that clicking the button causes the button to become disabled.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(test_api_.GetReselectButton()).NotifyClick(event);
  EXPECT_FALSE(test_api_.GetReselectButton()->GetEnabled());

  // Simulate a selection and verify that the reselect button can get re-enabled
  // again.
  media_lists_[DesktopMediaList::Type::kScreen]
      ->OnDelegatedSourceListSelection();
  EXPECT_TRUE(test_api_.GetReselectButton()->GetEnabled());
}

// Verifies that clicking the Reselect button will cause the delegated source
// list to be triggered to show again.
TEST_F(DelegatedSourceListTest, ReselectTriggersShowDelegatedSourceList) {
  SetSourceTypes(
      {DesktopMediaList::Type::kWebContents},
      {DesktopMediaList::Type::kScreen, DesktopMediaList::Type::kWindow});
  CreatePickerViews(/*request_audio=*/false,
                    /*screen_exclude_system_audio=*/true,
                    blink::mojom::WindowAudioPreference::kExclude);

  // ClearSourceListSelection should not have been called on either list yet.
  EXPECT_EQ(0, media_lists_[DesktopMediaList::Type::kScreen]
                   ->clear_delegated_source_list_selection_count());
  EXPECT_EQ(0, media_lists_[DesktopMediaList::Type::kWindow]
                   ->clear_delegated_source_list_selection_count());

  // Ensure that we have an enabled source list button.
  test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
  media_lists_[DesktopMediaList::Type::kScreen]
      ->OnDelegatedSourceListSelection();
  ASSERT_NE(nullptr, test_api_.GetReselectButton());
  EXPECT_TRUE(test_api_.GetReselectButton()->GetEnabled());

  // Verify that clicking the button causes the selection to be cleared on the
  // current source list.
  const ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(test_api_.GetReselectButton()).NotifyClick(event);
  EXPECT_EQ(1, media_lists_[DesktopMediaList::Type::kScreen]
                   ->clear_delegated_source_list_selection_count());
  EXPECT_EQ(0, media_lists_[DesktopMediaList::Type::kWindow]
                   ->clear_delegated_source_list_selection_count());
}

#endif  // BUILDFLAG(IS_MAC)
}  // namespace views

#if BUILDFLAG(IS_MAC)

// A fake implementation of AudioCapturePermissionChecker for testing.
class FakeAudioCapturePermissionChecker : public AudioCapturePermissionChecker {
 public:
  FakeAudioCapturePermissionChecker() = default;
  ~FakeAudioCapturePermissionChecker() override = default;

  // AudioCapturePermissionChecker implementation.
  State GetState() const override { return state_; }
  void RunCheck() override { run_check_called_ = true; }

  void SetState(State state) { state_ = state; }
  bool run_check_called() const { return run_check_called_; }
  void ResetRunCheckCalled() { run_check_called_ = false; }

 private:
  State state_ = State::kUnknown;
  bool run_check_called_ = false;
};

class DesktopMediaPickerAudioPermissionTest
    : public views::DesktopMediaPickerViewsTestBase {
 public:
  DesktopMediaPickerAudioPermissionTest()
      : DesktopMediaPickerViewsTestBase({DesktopMediaList::Type::kScreen}) {}
  ~DesktopMediaPickerAudioPermissionTest() override = default;

  void SetUp() override {
    DesktopMediaPickerViewsTestBase::SetUp();
    if (!media::IsMacCatapSystemLoopbackCaptureSupported()) {
      GTEST_SKIP() << "AudioCapturePermissionChecker only created if Catap "
                      "loopback capture is supported.";
    }

    test_api_.SelectTabForSourceType(DesktopMediaList::Type::kScreen);
    test_api_.SetAudioSharingApprovedByUser(
        true);  // Default to approved for these tests.

    auto fake_audio_permission_checker =
        std::make_unique<FakeAudioCapturePermissionChecker>();
    fake_audio_permission_checker_ = fake_audio_permission_checker.get();
    GetPickerDialogView()->SetAudioCapturePermissionCheckerForTest(
        std::move(fake_audio_permission_checker));

    pane_ = test_api_.GetActivePane();
    ASSERT_TRUE(pane_);
  }

  void TearDown() override {
    fake_audio_permission_checker_ = nullptr;
    pane_ = nullptr;
    DesktopMediaPickerViewsTestBase::TearDown();
  }

  void AudioSharingApprovedByUserUpdate() {
    GetPickerDialogView()->OnAudioSharingApprovedByUserUpdate();
  }

  void TriggerOnAudioPermissionUpdate() {
    GetPickerDialogView()->OnAudioPermissionUpdate();
  }

 protected:
  raw_ptr<DesktopMediaPaneView> pane_ = nullptr;
  raw_ptr<FakeAudioCapturePermissionChecker> fake_audio_permission_checker_ =
      nullptr;
};

TEST_F(DesktopMediaPickerAudioPermissionTest,
       OnTriggerUnknownPermissionAndAudioApproved) {
  fake_audio_permission_checker_->SetState(
      AudioCapturePermissionChecker::State::kUnknown);
  pane_->SetAudioSharingApprovedByUser(true);  // Ensure audio is approved

  // Call the method under test.
  AudioSharingApprovedByUserUpdate();

  EXPECT_TRUE(fake_audio_permission_checker_->run_check_called());
  EXPECT_FALSE(pane_->IsAudioWarningVisible());
}

TEST_F(DesktopMediaPickerAudioPermissionTest,
       OnTriggerDeniedPermissionAndAudioApproved) {
  fake_audio_permission_checker_->SetState(
      AudioCapturePermissionChecker::State::kDenied);
  pane_->SetAudioSharingApprovedByUser(true);  // Ensure audio is approved

  // Call the method under test.
  AudioSharingApprovedByUserUpdate();

  EXPECT_FALSE(fake_audio_permission_checker_->run_check_called());
  EXPECT_TRUE(pane_->IsAudioWarningVisible());
}

TEST_F(DesktopMediaPickerAudioPermissionTest,
       OnTriggerGrantedPermissionAndAudioApproved) {
  fake_audio_permission_checker_->SetState(
      AudioCapturePermissionChecker::State::kGranted);
  pane_->SetAudioSharingApprovedByUser(true);

  // Call the method under test.
  AudioSharingApprovedByUserUpdate();

  EXPECT_FALSE(fake_audio_permission_checker_->run_check_called());
  EXPECT_FALSE(pane_->IsAudioWarningVisible());
}

TEST_F(DesktopMediaPickerAudioPermissionTest,
       OnTriggerDeniedPermissionAndAudioNotApproved) {
  fake_audio_permission_checker_->SetState(
      AudioCapturePermissionChecker::State::kDenied);
  pane_->SetAudioSharingApprovedByUser(false);

  // Call the method under test.
  AudioSharingApprovedByUserUpdate();

  EXPECT_FALSE(fake_audio_permission_checker_->run_check_called());
  EXPECT_FALSE(pane_->IsAudioWarningVisible());
}

TEST_F(DesktopMediaPickerAudioPermissionTest,
       OnUpdateGrantedPermissionAndAudioApproved) {
  fake_audio_permission_checker_->SetState(
      AudioCapturePermissionChecker::State::kGranted);
  pane_->SetAudioSharingApprovedByUser(true);

  // Call the method under test.
  TriggerOnAudioPermissionUpdate();

  EXPECT_FALSE(pane_->IsAudioWarningVisible());
}

TEST_F(DesktopMediaPickerAudioPermissionTest,
       OnUpdateDeniedPermissionAndAudioApproved) {
  fake_audio_permission_checker_->SetState(
      AudioCapturePermissionChecker::State::kDenied);
  pane_->SetAudioSharingApprovedByUser(true);

  // Call the method under test.
  TriggerOnAudioPermissionUpdate();

  EXPECT_TRUE(pane_->IsAudioWarningVisible());
}

TEST_F(DesktopMediaPickerAudioPermissionTest,
       OnUpdateDeniedPermissionAndAudioNotApproved) {
  fake_audio_permission_checker_->SetState(
      AudioCapturePermissionChecker::State::kDenied);
  pane_->SetAudioSharingApprovedByUser(false);

  // Call the method under test.
  TriggerOnAudioPermissionUpdate();

  EXPECT_FALSE(pane_->IsAudioWarningVisible());
}

#endif  // BUILDFLAG(IS_MAC)
