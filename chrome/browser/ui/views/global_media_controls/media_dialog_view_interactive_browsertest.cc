// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view_observer.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/media_message_center/media_notification_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/media_start_stop_observer.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/controls/button/image_button.h"

using media_session::mojom::MediaSessionAction;

namespace {

class MediaToolbarButtonWatcher : public MediaToolbarButtonObserver,
                                  public MediaDialogViewObserver {
 public:
  explicit MediaToolbarButtonWatcher(MediaToolbarButtonView* button)
      : button_(button) {
    button_->AddObserver(this);
  }

  ~MediaToolbarButtonWatcher() override {
    button_->RemoveObserver(this);
    if (observed_dialog_ != nullptr &&
        observed_dialog_ == MediaDialogView::GetDialogViewForTesting()) {
      observed_dialog_->RemoveObserver(this);
    }
  }

  // MediaDialogViewObserver implementation.
  void OnMediaSessionShown() override { CheckDialogForText(); }

  void OnMediaSessionHidden() override {}

  void OnMediaSessionMetadataUpdated() override { CheckDialogForText(); }

  // MediaToolbarButtonObserver implementation.
  void OnMediaDialogOpened() override {
    waiting_for_dialog_opened_ = false;
    MaybeStopWaiting();
  }

  void OnMediaButtonShown() override {
    waiting_for_button_shown_ = false;
    MaybeStopWaiting();
  }

  void OnMediaButtonHidden() override {}
  void OnMediaButtonEnabled() override {}
  void OnMediaButtonDisabled() override {}

  void WaitForDialogOpened() {
    if (MediaDialogView::IsShowing())
      return;
    waiting_for_dialog_opened_ = true;
    Wait();
  }

  void WaitForButtonShown() {
    if (button_->GetVisible())
      return;
    waiting_for_button_shown_ = true;
    Wait();
  }

  void WaitForDialogToContainText(const base::string16& text) {
    if (DialogContainsText(text))
      return;

    waiting_for_dialog_to_contain_text_ = true;
    expected_text_ = text;
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

  void MaybeStopWaiting() {
    if (!run_loop_)
      return;

    if (!waiting_for_dialog_opened_ && !waiting_for_button_shown_ &&
        !waiting_for_dialog_to_contain_text_) {
      run_loop_->Quit();
    }
  }

  void Wait() {
    ASSERT_EQ(nullptr, run_loop_.get());
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // Checks the title and artist of each notification in the dialog to see if
  // |text| is contained anywhere in the dialog.
  bool DialogContainsText(const base::string16& text) {
    for (const auto notification_pair :
         MediaDialogView::GetDialogViewForTesting()
             ->GetNotificationsForTesting()) {
      const media_message_center::MediaNotificationView* view =
          notification_pair.second->view_for_testing();
      if (view->title_label_for_testing()->GetText().find(text) !=
              std::string::npos ||
          view->artist_label_for_testing()->GetText().find(text) !=
              std::string::npos) {
        return true;
      }
    }
    return false;
  }

  MediaToolbarButtonView* const button_;
  std::unique_ptr<base::RunLoop> run_loop_;

  bool waiting_for_dialog_opened_ = false;
  bool waiting_for_button_shown_ = false;

  MediaDialogView* observed_dialog_ = nullptr;
  bool waiting_for_dialog_to_contain_text_ = false;
  base::string16 expected_text_;

  DISALLOW_COPY_AND_ASSIGN(MediaToolbarButtonWatcher);
};

}  // anonymous namespace

class MediaDialogViewBrowserTest : public InProcessBrowserTest {
 public:
  MediaDialogViewBrowserTest() = default;
  ~MediaDialogViewBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void SetUp() override {
    feature_list_.InitAndEnableFeature(media::kGlobalMediaControls);
    InProcessBrowserTest::SetUp();
  }

  MediaToolbarButtonView* GetToolbarIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->media_button();
  }

  void ClickToolbarIcon() { ClickButton(GetToolbarIcon()); }

  bool IsToolbarIconVisible() { return GetToolbarIcon()->GetVisible(); }

  void WaitForVisibleToolbarIcon() {
    MediaToolbarButtonWatcher(GetToolbarIcon()).WaitForButtonShown();
  }

  void OpenTestURL() {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("media/session")),
        base::FilePath(FILE_PATH_LITERAL("video-with-metadata.html")));
    ui_test_utils::NavigateToURL(browser(), url);
  }

  void OpenDifferentMetadataURLInNewTab() {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("media/session")),
        base::FilePath(
            FILE_PATH_LITERAL("video-with-different-metadata.html")));
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  }

  void StartPlayback() {
    // The test HTML files used in these tests contain "play()" functions that
    // play the video.
    GetActiveWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("play()"), base::NullCallback());
  }

  void WaitForStart() {
    content::MediaStartStopObserver observer(
        GetActiveWebContents(), content::MediaStartStopObserver::Type::kStart);
    observer.Wait();
  }

  void WaitForStop() {
    content::MediaStartStopObserver observer(
        GetActiveWebContents(), content::MediaStartStopObserver::Type::kStop);
    observer.Wait();
  }

  void WaitForDialogOpened() {
    MediaToolbarButtonWatcher(GetToolbarIcon()).WaitForDialogOpened();
  }

  bool IsDialogVisible() { return MediaDialogView::IsShowing(); }

  void WaitForDialogToContainText(const base::string16& text) {
    MediaToolbarButtonWatcher(GetToolbarIcon())
        .WaitForDialogToContainText(text);
  }

  void ClickPauseButtonOnDialog() {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(MediaDialogView::IsShowing());
    ClickButton(GetButtonForAction(MediaSessionAction::kPause));
  }

  void ClickPlayButtonOnDialog() {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(MediaDialogView::IsShowing());
    ClickButton(GetButtonForAction(MediaSessionAction::kPlay));
  }

  void ClickNotificationByTitle(const base::string16& title) {
    ASSERT_TRUE(MediaDialogView::IsShowing());
    MediaNotificationContainerImplView* notification =
        GetNotificationByTitle(title);
    ASSERT_NE(nullptr, notification);
    ClickButton(notification);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  void ClickButton(views::Button* button) {
    base::RunLoop closure_loop;
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        closure_loop.QuitClosure());
    closure_loop.Run();
  }

  views::ImageButton* GetButtonForAction(MediaSessionAction action) {
    return GetButtonForAction(
        MediaDialogView::GetDialogViewForTesting()->children().front(),
        static_cast<int>(action));
  }

  // Recursively tries to find a views::ImageButton for the given
  // MediaSessionAction. This operates under the assumption that
  // media_message_center::MediaNotificationView sets the tags of its action
  // buttons to the MediaSessionAction value.
  views::ImageButton* GetButtonForAction(views::View* view, int action) {
    if (view->GetClassName() == views::ImageButton::kViewClassName) {
      views::ImageButton* image_button = static_cast<views::ImageButton*>(view);
      if (image_button->tag() == action)
        return image_button;
    }

    for (views::View* child : view->children()) {
      views::ImageButton* result = GetButtonForAction(child, action);
      if (result)
        return result;
    }

    return nullptr;
  }

  // Finds a MediaNotificationContainerImplView by title.
  MediaNotificationContainerImplView* GetNotificationByTitle(
      const base::string16& title) {
    for (const auto notification_pair :
         MediaDialogView::GetDialogViewForTesting()
             ->GetNotificationsForTesting()) {
      const media_message_center::MediaNotificationView* view =
          notification_pair.second->view_for_testing();
      if (view->title_label_for_testing()->GetText() == title)
        return notification_pair.second;
    }
    return nullptr;
  }

  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MediaDialogViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       ShowsMetadataAndControlsMedia) {
  // The toolbar icon should not start visible.
  EXPECT_FALSE(IsToolbarIconVisible());

  // Opening a page with media that hasn't played yet should not make the
  // toolbar icon visible.
  OpenTestURL();
  EXPECT_FALSE(IsToolbarIconVisible());

  // Once playback starts, the icon should be visible, but the dialog should not
  // appear if it hasn't been clicked.
  StartPlayback();
  WaitForStart();
  WaitForVisibleToolbarIcon();
  EXPECT_TRUE(IsToolbarIconVisible());
  EXPECT_FALSE(IsDialogVisible());

  // Clicking on the toolbar icon should open the dialog.
  ClickToolbarIcon();
  WaitForDialogOpened();
  EXPECT_TRUE(IsDialogVisible());

  // The dialog should contain the title and artist. These are taken from
  // video-with-metadata.html.
  WaitForDialogToContainText(base::ASCIIToUTF16("Big Buck Bunny"));
  WaitForDialogToContainText(base::ASCIIToUTF16("Blender Foundation"));

  // Clicking on the pause button in the dialog should pause the media on the
  // page.
  ClickPauseButtonOnDialog();
  WaitForStop();

  // Clicking on the play button in the dialog should play the media on the
  // page.
  ClickPlayButtonOnDialog();
  WaitForStart();

  // Clicking on the toolbar icon again should hide the dialog.
  EXPECT_TRUE(IsDialogVisible());
  ClickToolbarIcon();
  EXPECT_FALSE(IsDialogVisible());
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest, ShowsMultipleMediaSessions) {
  // Open a tab and play media.
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  // Open another tab and play different media.
  OpenDifferentMetadataURLInNewTab();
  StartPlayback();
  WaitForStart();

  // Open the media dialog.
  ClickToolbarIcon();
  WaitForDialogOpened();
  EXPECT_TRUE(IsDialogVisible());

  // The dialog should show both media sessions.
  WaitForDialogToContainText(base::ASCIIToUTF16("Big Buck Bunny"));
  WaitForDialogToContainText(base::ASCIIToUTF16("Blender Foundation"));
  WaitForDialogToContainText(base::ASCIIToUTF16("Different Title"));
  WaitForDialogToContainText(base::ASCIIToUTF16("Another Artist"));
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       ClickingOnNotificationGoesBackToTab) {
  // Open a tab and play media.
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  // Pointer to the first tab.
  content::WebContents* first_web_contents = GetActiveWebContents();

  // Open another tab and play different media.
  OpenDifferentMetadataURLInNewTab();
  StartPlayback();
  WaitForStart();

  // Now the active web contents is the second tab.
  content::WebContents* second_web_contents = GetActiveWebContents();
  ASSERT_NE(first_web_contents, second_web_contents);

  // Open the media dialog.
  ClickToolbarIcon();
  WaitForDialogOpened();
  EXPECT_TRUE(IsDialogVisible());

  // Wait for the dialog to be populated.
  WaitForDialogToContainText(base::ASCIIToUTF16("Big Buck Bunny"));
  WaitForDialogToContainText(base::ASCIIToUTF16("Different Title"));

  // The second tab should be the active tab.
  EXPECT_EQ(second_web_contents, GetActiveWebContents());

  // Clicking the first notification should make the first tab active.
  ClickNotificationByTitle(base::ASCIIToUTF16("Big Buck Bunny"));
  EXPECT_EQ(first_web_contents, GetActiveWebContents());
}
