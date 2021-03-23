// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/get_current_browsing_context_media_dialog.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/web_modal/test_web_contents_modal_dialog_host.h"
#include "components/web_modal/test_web_contents_modal_dialog_manager_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/web_dialogs/web_dialog_web_contents_delegate.h"

// All tests are flaky on Win10: https://crbug.com/1181150
#if !defined(OS_WIN)
namespace {
#if defined(OS_MAC)
constexpr ui::KeyboardCode kDefaultKey = ui::VKEY_SPACE;
#else
constexpr ui::KeyboardCode kDefaultKey = ui::VKEY_RETURN;
#endif
}  // namespace
#endif

enum AutoAction { kNone, kAutoAccept, kAutoReject };

namespace views {

class MockDialogObserver : public DesktopMediaPickerManager::DialogObserver {
 public:
  MOCK_METHOD(void, OnDialogOpened, (), (override));
  MOCK_METHOD(void, OnDialogClosed, (), (override));
};

class MockWebContentsDelegate : public content::WebContentsDelegate {};

class GetCurrentBrowsingContextMediaDialogTest
    : public BrowserWithTestWindowTest {
 public:
  void CustomSetUp(bool request_audio,
                   bool approve_audio_by_default,
                   bool is_closed_called = true,
                   AutoAction auto_action = kNone) {
#if defined(OS_MAC)
    // These tests create actual child Widgets, which normally have a closure
    // animation on Mac; inhibit it here to avoid the tests flakily hanging.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableModalAnimations);
#endif
    DesktopMediaPickerManager::Get()->AddObserver(&mock_dialog_observer_);
    AddTab(browser(), GURL(url::kAboutBlankURL));

    web_contents_ = browser()->tab_strip_model()->DetachWebContentsAt(0);
    web_contents_->SetDelegate(&web_delegate_);

    // Creates the parent widget which is needed for creating child widgets.
    auto dialog_delegate = std::make_unique<views::DialogDelegateView>();
    dialog_delegate->SetModalType(ui::MODAL_TYPE_WINDOW);
    parent_widget_ = views::DialogDelegate::CreateDialogWidget(
        dialog_delegate.release(), GetContext(), nullptr);

    // Creates and sets the dialog host.
    dialog_host_ = std::make_unique<web_modal::TestWebContentsModalDialogHost>(
        parent_widget_->GetNativeView());
    dialog_host_->set_max_dialog_size(gfx::Size(500, 500));
    manager_delegate_.set_web_contents_modal_dialog_host(dialog_host_.get());

    // Sets delegate for web_contents_.
    web_modal::WebContentsModalDialogManager::CreateForWebContents(
        web_contents_.get());
    auto* manager = web_modal::WebContentsModalDialogManager::FromWebContents(
        web_contents_.get());
    manager->SetDelegate(&manager_delegate_);

    DesktopMediaPicker::Params dialog_params;
    // Sets the parameters for the confirmation dialog.
    dialog_params.web_contents = web_contents_.get();
    dialog_params.context = GetContext();
    dialog_params.parent = parent_widget_->GetNativeWindow();
    dialog_params.app_name = u"OriginApp";
    dialog_params.target_name = u"TargetApp";
    dialog_params.request_audio = request_audio;
    dialog_params.approve_audio_by_default = approve_audio_by_default;

    EXPECT_CALL(mock_dialog_observer_, OnDialogOpened()).Times(1);
    EXPECT_CALL(mock_dialog_observer_, OnDialogClosed())
        .Times(is_closed_called ? 1 : 0);

    if (auto_action == kAutoAccept) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kThisTabCaptureAutoAccept);
    }
    if (auto_action == kAutoReject) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kThisTabCaptureAutoReject);
    }

    dialog_ = std::make_unique<GetCurrentBrowsingContextMediaDialog>();
    dialog_->Show(
        dialog_params, {},
        base::BindOnce(&GetCurrentBrowsingContextMediaDialogTest::OnDialogDone,
                       base::Unretained(this)));
    render_process_id_ = web_contents_->GetMainFrame()->GetProcess()->GetID();
    render_frame_id_ = web_contents_->GetMainFrame()->GetRoutingID();
  }

  void TearDown() override {
    DesktopMediaPickerManager::Get()->RemoveObserver(&mock_dialog_observer_);
    parent_widget_->CloseWithReason(Widget::ClosedReason::kUnspecified);
    web_contents_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  base::Optional<content::DesktopMediaID> WaitForDialogDone() {
    run_loop_.Run();
    return dialog_id_;
  }

  void DoubleTapShareButton() {
    ui::GestureEventDetails details(ui::ET_GESTURE_TAP);
    details.set_tap_count(2);
    ui::GestureEvent double_tap(/*x=*/10, /*y=*/10, /*flags=*/0,
                                base::TimeTicks(), details);
    dialog_->GetHostForTesting()->GetOkButton()->OnGestureEvent(&double_tap);
  }

  void SimulateKeyPress(ui::KeyboardCode key) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key, ui::EF_NONE);
    dialog_->GetHostForTesting()->GetWidget()->OnKeyEvent(&event);
  }

  void OnDialogDone(content::DesktopMediaID dialog_id) {
    dialog_id_ = dialog_id;
    run_loop_.Quit();
  }

 protected:
  base::Optional<content::DesktopMediaID> dialog_id_;
  int render_process_id_ = MSG_ROUTING_NONE;
  int render_frame_id_ = MSG_ROUTING_NONE;
  std::unique_ptr<content::WebContents> web_contents_;
  // dialog_ is responsible for creating the confirmation dialog and is
  // used to access the host for testing.
  std::unique_ptr<GetCurrentBrowsingContextMediaDialog> dialog_;
  MockDialogObserver mock_dialog_observer_;
  MockWebContentsDelegate web_delegate_;
  std::unique_ptr<web_modal::TestWebContentsModalDialogHost> dialog_host_;
  web_modal::TestWebContentsModalDialogManagerDelegate manager_delegate_;
  Widget* parent_widget_;
  base::RunLoop run_loop_;
};

// All tests are flaky on Win10: https://crbug.com/1181150
#if !defined(OS_WIN)
TEST_F(GetCurrentBrowsingContextMediaDialogTest, CancelButtonAlwaysEnabled) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/true,
              /*is_closed_called=*/false);
  EXPECT_TRUE(dialog_->GetHostForTesting()->IsDialogButtonEnabled(
      ui::DIALOG_BUTTON_CANCEL));
}

TEST_F(GetCurrentBrowsingContextMediaDialogTest, ShareButtonAlwaysEnabled) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/true,
              /*is_closed_called=*/false);
  EXPECT_TRUE(dialog_->GetHostForTesting()->IsDialogButtonEnabled(
      ui::DIALOG_BUTTON_OK));
}

TEST_F(GetCurrentBrowsingContextMediaDialogTest, DefaultAudioSelection) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/true);
  content::DesktopMediaID kResultId(
      content::DesktopMediaID::TYPE_WEB_CONTENTS, 0,
      content::WebContentsMediaCaptureId(render_process_id_, render_frame_id_));
  kResultId.audio_share = true;
  dialog_->GetHostForTesting()->AcceptDialog();
  EXPECT_EQ(kResultId, WaitForDialogDone());
}

TEST_F(GetCurrentBrowsingContextMediaDialogTest,
       DoneCallbackCalledWhenWindowClosed) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/true);
  dialog_->GetHostForTesting()->Close();
  EXPECT_EQ(content::DesktopMediaID(), WaitForDialogDone());
}

TEST_F(GetCurrentBrowsingContextMediaDialogTest,
       DoneCallbackCalledWhenWindowClosedWithoutCheckboxTicked) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/false);
  dialog_->GetHostForTesting()->Close();
  EXPECT_EQ(content::DesktopMediaID(), WaitForDialogDone());
}

// Verifies that audio share information is recorded if the checkbox
// is checked.
TEST_F(GetCurrentBrowsingContextMediaDialogTest,
       DoneCallbackCalledWithAudioShare) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/true);
  content::DesktopMediaID kResultId(
      content::DesktopMediaID::TYPE_WEB_CONTENTS, 0,
      content::WebContentsMediaCaptureId(render_process_id_, render_frame_id_));
  kResultId.audio_share = true;
  dialog_->GetHostForTesting()->AcceptDialog();
  EXPECT_EQ(kResultId, WaitForDialogDone());
}

// Verifies that audio share information is recorded if there is no checkbox.
TEST_F(GetCurrentBrowsingContextMediaDialogTest,
       DoneCallbackCalledWithNoAudioShare) {
  CustomSetUp(/*request_audio=*/false, /*approve_audio_by_default=*/true);
  content::DesktopMediaID kResultId(
      content::DesktopMediaID::TYPE_WEB_CONTENTS, 0,
      content::WebContentsMediaCaptureId(render_process_id_, render_frame_id_));
  dialog_->GetHostForTesting()->AcceptDialog();
  EXPECT_EQ(kResultId, WaitForDialogDone());
}

// Verifies that audio share information is recorded if the checkbox
// is not checked.
TEST_F(GetCurrentBrowsingContextMediaDialogTest,
       DoneCallbackCalledWithAudioShareFalse) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/false);
  content::DesktopMediaID kResultId(
      content::DesktopMediaID::TYPE_WEB_CONTENTS, 0,
      content::WebContentsMediaCaptureId(render_process_id_, render_frame_id_));
  dialog_->GetHostForTesting()->AcceptDialog();
  EXPECT_EQ(kResultId, WaitForDialogDone());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(GetCurrentBrowsingContextMediaDialogTest, PressingDefaultButtonCancels) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/true);
  SimulateKeyPress(kDefaultKey);
  EXPECT_EQ(content::DesktopMediaID(), WaitForDialogDone());
}
#endif

TEST_F(GetCurrentBrowsingContextMediaDialogTest, ShareButtonAccepts) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/true);
  content::DesktopMediaID kResultId(
      content::DesktopMediaID::TYPE_WEB_CONTENTS, 0,
      content::WebContentsMediaCaptureId(render_process_id_, render_frame_id_));
  kResultId.audio_share = true;
  dialog_->GetHostForTesting()->GetOkButton()->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, kDefaultKey, 0));
  EXPECT_EQ(kResultId, WaitForDialogDone());
}

TEST_F(GetCurrentBrowsingContextMediaDialogTest, DoubleTapOnShare) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/true);
  content::DesktopMediaID kResultId(
      content::DesktopMediaID::TYPE_WEB_CONTENTS, 0,
      content::WebContentsMediaCaptureId(render_process_id_, render_frame_id_));
  kResultId.audio_share = true;
  DoubleTapShareButton();
  EXPECT_EQ(kResultId, WaitForDialogDone());
}

TEST_F(GetCurrentBrowsingContextMediaDialogTest, AutoAcceptTabCapture) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/true,
              /*is_closed_called=*/true, /*auto_action=*/kAutoAccept);
  content::DesktopMediaID kResultId(
      content::DesktopMediaID::TYPE_WEB_CONTENTS, 0,
      content::WebContentsMediaCaptureId(render_process_id_, render_frame_id_));
  kResultId.audio_share = true;
  EXPECT_EQ(kResultId, WaitForDialogDone());
}

TEST_F(GetCurrentBrowsingContextMediaDialogTest, AutoRejectTabCapture) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/true,
              /*is_closed_called=*/true, /*auto_action=*/kAutoReject);
  EXPECT_EQ(content::DesktopMediaID(), WaitForDialogDone());
}

// Validates that the cancel button is initially focused and enabled.
TEST_F(GetCurrentBrowsingContextMediaDialogTest, InitiallyFocusesCancel) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/true,
              /*is_closed_called=*/false);
  EXPECT_EQ(dialog_->GetHostForTesting()->GetCancelButton(),
            dialog_->GetHostForTesting()->GetInitiallyFocusedView());
}

// Validate that the title of the confirmation box shows the correct text.
TEST_F(GetCurrentBrowsingContextMediaDialogTest,
       ConfirmationBoxShowsCorrectTitle) {
  CustomSetUp(/*request_audio=*/true, /*approve_audio_by_default=*/true,
              /*is_closed_called=*/false);
  EXPECT_EQ(dialog_->GetHostForTesting()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_GET_CURRENT_BROWSING_CONTEXT_MEDIA_DIALOG_TITLE));
}
#endif

}  // namespace views
