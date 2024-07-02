// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_ui_for_test.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view_observer.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_item_manager_observer.h"
#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"

namespace {

using media_session::mojom::MediaSessionAction;

class MediaToolbarButtonWatcher
    : public MediaToolbarButtonObserver,
      public MediaDialogViewObserver,
      public global_media_controls::MediaItemManagerObserver {
 public:
  MediaToolbarButtonWatcher(
      MediaToolbarButtonView* button,
      global_media_controls::MediaItemManager* item_manager)
      : button_(button), item_manager_(item_manager) {
    button_->AddObserver(this);
    item_manager_->AddObserver(this);
  }

  MediaToolbarButtonWatcher(const MediaToolbarButtonWatcher&) = delete;
  MediaToolbarButtonWatcher& operator=(const MediaToolbarButtonWatcher&) =
      delete;

  ~MediaToolbarButtonWatcher() override {
    button_->RemoveObserver(this);
    item_manager_->RemoveObserver(this);
    if (observed_dialog_ &&
        observed_dialog_ == MediaDialogView::GetDialogViewForTesting()) {
      observed_dialog_->RemoveObserver(this);
    }
  }

  // MediaDialogViewObserver implementation.
  void OnMediaSessionShown() override { CheckDialogForText(); }

  void OnMediaSessionHidden() override {}

  void OnMediaSessionMetadataUpdated() override { CheckDialogForText(); }

  void OnMediaSessionActionsChanged() override {
    CheckPictureInPictureButton();
  }

  // MediaToolbarButtonObserver implementation.
  void OnMediaDialogOpened() override {
    waiting_for_dialog_opened_ = false;
    MaybeStopWaiting();
  }

  void OnMediaButtonShown() override {
    waiting_for_button_shown_ = false;
    MaybeStopWaiting();
  }

  void OnMediaButtonHidden() override {
    waiting_for_button_hidden_ = false;
    MaybeStopWaiting();
  }

  void OnMediaButtonEnabled() override {}
  void OnMediaButtonDisabled() override {}

  // MediaItemManagerObserver implementation. OnMediaDialogOpened() is shared
  // with the MediaToolbarButtonObserver interface.
  void OnItemListChanged() override {}
  void OnMediaDialogClosed() override {}

  [[nodiscard]] bool WaitForDialogOpened() {
    if (MediaDialogView::IsShowing())
      return true;
    waiting_for_dialog_opened_ = true;
    Wait();
    return MediaDialogView::IsShowing();
  }

  [[nodiscard]] bool WaitForButtonShown() {
    if (button_->GetVisible())
      return true;
    waiting_for_button_shown_ = true;
    Wait();
    return button_->GetVisible();
  }

  [[nodiscard]] bool WaitForButtonHidden() {
    if (!button_->GetVisible())
      return true;
    waiting_for_button_hidden_ = true;
    Wait();
    return !button_->GetVisible();
  }

  void WaitForDialogToContainText(const std::u16string& text) {
    if (DialogContainsText(text))
      return;

    waiting_for_dialog_to_contain_text_ = true;
    expected_text_ = text;
    observed_dialog_ = MediaDialogView::GetDialogViewForTesting();
    observed_dialog_->AddObserver(this);
    Wait();
  }

  void WaitForItemCount(int count) {
    if (GetItemCount() == count)
      return;

    waiting_for_item_count_ = true;
    expected_item_count_ = count;
    observed_dialog_ = MediaDialogView::GetDialogViewForTesting();
    observed_dialog_->AddObserver(this);
    Wait();
  }

  void WaitForPictureInPictureButtonVisibility(bool visible) {
    if (CheckPictureInPictureButtonVisibility(visible))
      return;

    waiting_for_pip_visibility_changed_ = true;
    expected_pip_visibility_ = visible;
    observed_dialog_ = MediaDialogView::GetDialogViewForTesting();
    observed_dialog_->AddObserver(this);
    Wait();
  }

 private:
  void CheckDialogForText() {
    if (!waiting_for_dialog_to_contain_text_)
      return;

    if (!DialogContainsText(expected_text_))
      return;

    waiting_for_dialog_to_contain_text_ = false;
    MaybeStopWaiting();
  }

  void CheckItemCount() {
    if (!waiting_for_item_count_)
      return;

    if (GetItemCount() != expected_item_count_)
      return;

    waiting_for_item_count_ = false;
    MaybeStopWaiting();
  }

  void CheckPictureInPictureButton() {
    if (!waiting_for_pip_visibility_changed_)
      return;

    if (!CheckPictureInPictureButtonVisibility(expected_pip_visibility_))
      return;

    waiting_for_pip_visibility_changed_ = false;
    MaybeStopWaiting();
  }

  void MaybeStopWaiting() {
    if (!run_loop_)
      return;

    if (!waiting_for_dialog_opened_ && !waiting_for_button_shown_ &&
        !waiting_for_dialog_to_contain_text_ && !waiting_for_item_count_ &&
        !waiting_for_pip_visibility_changed_) {
      run_loop_->Quit();
    }
  }

  void Wait() {
    ASSERT_EQ(nullptr, run_loop_.get());
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // Checks the label texts of each media item to see if |text| is contained
  // anywhere in the dialog.
  bool DialogContainsText(const std::u16string& text) {
    for (const auto& item_pair : MediaDialogView::GetDialogViewForTesting()
                                     ->GetUpdatedItemsForTesting()) {
      global_media_controls::MediaItemUIUpdatedView* view = item_pair.second;
      if (view->GetSourceLabelForTesting()->GetText().find(text) !=
              std::string::npos ||
          view->GetTitleLabelForTesting()->GetText().find(text) !=
              std::string::npos ||
          view->GetArtistLabelForTesting()->GetText().find(text) !=
              std::string::npos) {
        return true;
      }
    }
    return false;
  }

  bool CheckPictureInPictureButtonVisibility(bool visible) {
    global_media_controls::MediaItemUIUpdatedView* view =
        MediaDialogView::GetDialogViewForTesting()
            ->GetUpdatedItemsForTesting()
            .begin()
            ->second;
    global_media_controls::MediaActionButton* button =
        view->GetMediaActionButtonForTesting(
            MediaSessionAction::kEnterPictureInPicture);
    // The button should be invisible if it does not exist.
    if (!button) {
      return !visible;
    }
    return button->GetVisible() == visible;
  }

  int GetItemCount() {
    return MediaDialogView::GetDialogViewForTesting()
        ->GetUpdatedItemsForTesting()
        .size();
  }

  const raw_ptr<MediaToolbarButtonView> button_;
  std::unique_ptr<base::RunLoop> run_loop_;

  // We observe MediaItemManager for the dialog opening event. There may be a
  // better observation target than this.
  const raw_ptr<global_media_controls::MediaItemManager> item_manager_;

  bool waiting_for_dialog_opened_ = false;
  bool waiting_for_button_shown_ = false;
  bool waiting_for_button_hidden_ = false;
  bool waiting_for_item_count_ = false;
  bool waiting_for_pip_visibility_changed_ = false;

  raw_ptr<MediaDialogView> observed_dialog_ = nullptr;
  bool waiting_for_dialog_to_contain_text_ = false;
  std::u16string expected_text_;
  int expected_item_count_ = 0;
  bool expected_pip_visibility_ = false;
};

}  // namespace

MediaDialogUiForTest::MediaDialogUiForTest(
    base::RepeatingCallback<Browser*()> callback)
    : browser_callback_(callback) {}

MediaDialogUiForTest::~MediaDialogUiForTest() = default;

MediaToolbarButtonView* MediaDialogUiForTest::GetToolbarIcon() {
  LayoutBrowserIfNecessary();
  return BrowserView::GetBrowserViewForBrowser(browser_callback_.Run())
      ->toolbar()
      ->media_button();
}

void MediaDialogUiForTest::LayoutBrowserIfNecessary() {
  BrowserView::GetBrowserViewForBrowser(browser_callback_.Run())
      ->GetWidget()
      ->LayoutRootViewIfNecessary();
}

void MediaDialogUiForTest::ClickToolbarIcon() {
  CHECK(IsToolbarIconVisible());
  ui_test_utils::ClickOnView(GetToolbarIcon());
}

bool MediaDialogUiForTest::IsToolbarIconVisible() {
  return GetToolbarIcon()->GetVisible();
}

bool MediaDialogUiForTest::WaitForToolbarIconShown() {
  return MediaToolbarButtonWatcher(GetToolbarIcon(), GetItemManager())
      .WaitForButtonShown();
}

bool MediaDialogUiForTest::WaitForToolbarIconHidden() {
  return MediaToolbarButtonWatcher(GetToolbarIcon(), GetItemManager())
      .WaitForButtonHidden();
}

bool MediaDialogUiForTest::WaitForDialogOpened() {
  return MediaToolbarButtonWatcher(GetToolbarIcon(), GetItemManager())
      .WaitForDialogOpened();
}

bool MediaDialogUiForTest::IsDialogVisible() {
  return MediaDialogView::IsShowing();
}

void MediaDialogUiForTest::WaitForDialogToContainText(
    const std::u16string& text) {
  MediaToolbarButtonWatcher(GetToolbarIcon(), GetItemManager())
      .WaitForDialogToContainText(text);
}

void MediaDialogUiForTest::WaitForItemCount(int count) {
  MediaToolbarButtonWatcher(GetToolbarIcon(), GetItemManager())
      .WaitForItemCount(count);
}

void MediaDialogUiForTest::WaitForPictureInPictureButtonVisibility(
    bool visible) {
  MediaToolbarButtonWatcher(GetToolbarIcon(), GetItemManager())
      .WaitForPictureInPictureButtonVisibility(visible);
}

global_media_controls::MediaItemManager* MediaDialogUiForTest::GetItemManager()
    const {
  return MediaNotificationServiceFactory::GetForProfile(
             browser_callback_.Run()->profile())
      ->media_item_manager();
}
