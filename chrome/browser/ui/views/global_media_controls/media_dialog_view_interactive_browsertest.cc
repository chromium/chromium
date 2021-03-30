// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view_observer.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_list_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/new_badge_label.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/media_start_stop_observer.h"
#include "media/base/media_switches.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/view_utils.h"

using media_session::mojom::MediaSessionAction;

// Global Media Controls are not supported on Chrome OS.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)

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

  void WaitForDialogToContainText(const std::u16string& text) {
    if (DialogContainsText(text))
      return;

    waiting_for_dialog_to_contain_text_ = true;
    expected_text_ = text;
    observed_dialog_ = MediaDialogView::GetDialogViewForTesting();
    observed_dialog_->AddObserver(this);
    Wait();
  }

  void WaitForNotificationCount(int count) {
    if (GetNotificationCount() == count)
      return;

    waiting_for_notification_count_ = true;
    expected_notification_count_ = count;
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

  void CheckNotificationCount() {
    if (!waiting_for_notification_count_)
      return;

    if (GetNotificationCount() != expected_notification_count_)
      return;

    waiting_for_notification_count_ = false;
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
        !waiting_for_dialog_to_contain_text_ &&
        !waiting_for_notification_count_ &&
        !waiting_for_pip_visibility_changed_) {
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
  bool DialogContainsText(const std::u16string& text) {
    for (const auto& notification_pair :
         MediaDialogView::GetDialogViewForTesting()
             ->GetNotificationsForTesting()) {
      const media_message_center::MediaNotificationViewImpl* view =
          notification_pair.second->view_for_testing();
      if (view->title_label_for_testing()->GetText().find(text) !=
              std::string::npos ||
          view->artist_label_for_testing()->GetText().find(text) !=
              std::string::npos ||
          view->GetSourceTitleForTesting().find(text) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  bool CheckPictureInPictureButtonVisibility(bool visible) {
    const auto notification_pair = MediaDialogView::GetDialogViewForTesting()
                                       ->GetNotificationsForTesting()
                                       .begin();
    const media_message_center::MediaNotificationViewImpl* view =
        notification_pair->second->view_for_testing();

    return view->picture_in_picture_button_for_testing()->GetVisible() ==
           visible;
  }

  int GetNotificationCount() {
    return MediaDialogView::GetDialogViewForTesting()
        ->GetNotificationsForTesting()
        .size();
  }

  MediaToolbarButtonView* const button_;
  std::unique_ptr<base::RunLoop> run_loop_;

  bool waiting_for_dialog_opened_ = false;
  bool waiting_for_button_shown_ = false;
  bool waiting_for_notification_count_ = false;
  bool waiting_for_pip_visibility_changed_ = false;

  MediaDialogView* observed_dialog_ = nullptr;
  bool waiting_for_dialog_to_contain_text_ = false;
  std::u16string expected_text_;
  int expected_notification_count_ = 0;
  bool expected_pip_visibility_ = false;

  DISALLOW_COPY_AND_ASSIGN(MediaToolbarButtonWatcher);
};

class TestWebContentsPresentationManager
    : public media_router::WebContentsPresentationManager {
 public:
  void NotifyMediaRoutesChanged(
      const std::vector<media_router::MediaRoute>& routes) {
    for (auto& observer : observers_) {
      observer.OnMediaRoutesChanged(routes);
    }
  }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  MOCK_CONST_METHOD0(HasDefaultPresentationRequest, bool());
  MOCK_CONST_METHOD0(GetDefaultPresentationRequest,
                     const content::PresentationRequest&());
  MOCK_METHOD0(GetMediaRoutes, std::vector<media_router::MediaRoute>());

  void OnPresentationResponse(
      const content::PresentationRequest& presentation_request,
      media_router::mojom::RoutePresentationConnectionPtr connection,
      const media_router::RouteRequestResult& result) override {}

  base::WeakPtr<WebContentsPresentationManager> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<TestWebContentsPresentationManager> weak_factory_{this};
};

class TestMediaRouter : public media_router::MockMediaRouter {
 public:
  static std::unique_ptr<KeyedService> Create(
      content::BrowserContext* context) {
    return std::make_unique<TestMediaRouter>();
  }

  media_router::LoggerImpl* GetLogger() override {
    if (!logger_)
      logger_ = std::make_unique<media_router::LoggerImpl>();
    return logger_.get();
  }

  void RegisterMediaRoutesObserver(
      media_router::MediaRoutesObserver* observer) override {
    routes_observers_.push_back(observer);
  }

  void UnregisterMediaRoutesObserver(
      media_router::MediaRoutesObserver* observer) override {
    base::Erase(routes_observers_, observer);
  }

  void NotifyMediaRoutesChanged(
      const std::vector<media_router::MediaRoute>& routes) {
    for (auto* observer : routes_observers_)
      observer->OnRoutesUpdated(routes, {});
  }

 private:
  std::vector<media_router::MediaRoutesObserver*> routes_observers_;
  std::unique_ptr<media_router::LoggerImpl> logger_;
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
    // TODO(crbug.com/1182859): Update this test to enable the
    // kUseSodaForLiveCaption feature.
    feature_list_.InitWithFeatures(
        {media::kGlobalMediaControls, media::kGlobalMediaControlsForCast,
         media::kLiveCaption, feature_engagement::kIPHLiveCaptionFeature},
        {media::kUseSodaForLiveCaption});

    presentation_manager_ =
        std::make_unique<TestWebContentsPresentationManager>();
    media_router::WebContentsPresentationManager::SetTestInstance(
        presentation_manager_.get());

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    media_router::WebContentsPresentationManager::SetTestInstance(nullptr);
  }

  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &MediaDialogViewBrowserTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    media_router_ = static_cast<TestMediaRouter*>(
        media_router::ChromeMediaRouterFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                context, base::BindRepeating(&TestMediaRouter::Create)));
  }

  void LayoutBrowser() {
    BrowserView::GetBrowserViewForBrowser(browser())
        ->GetWidget()
        ->LayoutRootViewIfNecessary();
  }

  MediaToolbarButtonView* GetToolbarIcon() {
    LayoutBrowser();
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
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void StartPlayback() {
    // The test HTML files used in these tests contain "play()" functions that
    // play the video.
    GetActiveWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
        u"play()", base::NullCallback());
  }

  void WaitForStart() {
    content::MediaStartStopObserver observer(
        GetActiveWebContents(), content::MediaStartStopObserver::Type::kStart);
    observer.Wait();
  }

  void WaitForStop() { WaitForStop(GetActiveWebContents()); }

  void WaitForStop(content::WebContents* web_contents) {
    content::MediaStartStopObserver observer(
        web_contents, content::MediaStartStopObserver::Type::kStop);
    observer.Wait();
  }

  void DisablePictureInPicture() {
    GetActiveWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
        u"disablePictureInPicture()", base::NullCallback());
  }

  void EnablePictureInPicture() {
    GetActiveWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
        u"enablePictureInPicture()", base::NullCallback());
  }

  void WaitForEnterPictureInPicture() {
    content::MediaStartStopObserver observer(
        GetActiveWebContents(),
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    observer.Wait();
  }

  void WaitForExitPictureInPicture() {
    content::MediaStartStopObserver observer(
        GetActiveWebContents(),
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    observer.Wait();
  }

  void WaitForDialogOpened() {
    MediaToolbarButtonWatcher(GetToolbarIcon()).WaitForDialogOpened();
  }

  bool IsDialogVisible() { return MediaDialogView::IsShowing(); }

  void WaitForDialogToContainText(const std::u16string& text) {
    MediaToolbarButtonWatcher(GetToolbarIcon())
        .WaitForDialogToContainText(text);
  }

  void WaitForNotificationCount(int count) {
    MediaToolbarButtonWatcher(GetToolbarIcon()).WaitForNotificationCount(count);
  }

  void WaitForPictureInPictureButtonVisibility(bool visible) {
    MediaToolbarButtonWatcher(GetToolbarIcon())
        .WaitForPictureInPictureButtonVisibility(visible);
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

  void ClickEnterPictureInPictureButtonOnDialog() {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(MediaDialogView::IsShowing());
    ClickButton(GetButtonForAction(MediaSessionAction::kEnterPictureInPicture));
  }

  void ClickExitPictureInPictureButtonOnDialog() {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(MediaDialogView::IsShowing());
    ClickButton(GetButtonForAction(MediaSessionAction::kExitPictureInPicture));
  }

  void ClickEnableLiveCaptionOnDialog() {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(MediaDialogView::IsShowing());
    views::Button* live_caption_button = static_cast<views::Button*>(
        MediaDialogView::GetDialogViewForTesting()->live_caption_button_);
    ClickButton(live_caption_button);
  }

  void ClickNotificationByTitle(const std::u16string& title) {
    ASSERT_TRUE(MediaDialogView::IsShowing());
    MediaNotificationContainerImplView* notification =
        GetNotificationByTitle(title);
    ASSERT_NE(nullptr, notification);
    ClickButton(notification);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool IsPlayingSessionDisplayedFirst() {
    bool seen_paused = false;
    for (views::View* view : MediaDialogView::GetDialogViewForTesting()
                                 ->GetListViewForTesting()
                                 ->contents()
                                 ->children()) {
      MediaNotificationContainerImplView* notification =
          static_cast<MediaNotificationContainerImplView*>(view);

      if (seen_paused && notification->is_playing_for_testing())
        return false;

      if (!seen_paused && !notification->is_playing_for_testing())
        seen_paused = true;
    }

    return true;
  }

  views::ImageButton* GetButtonForAction(MediaSessionAction action) {
    return GetButtonForAction(
        MediaDialogView::GetDialogViewForTesting()->children().front(),
        static_cast<int>(action));
  }

  // Returns true if |target| exists in |base|'s forward focus chain
  bool ViewFollowsInFocusChain(views::View* base, views::View* target) {
    for (views::View* cur = base; cur; cur = cur->GetNextFocusableView()) {
      if (cur == target)
        return true;
    }
    return false;
  }

  views::Label* GetLiveCaptionTitleLabel() {
    return MediaDialogView::GetDialogViewForTesting()->live_caption_title_;
  }

  views::Label* GetLiveCaptionTitleNewBadgeLabel() {
    return MediaDialogView::GetDialogViewForTesting()
        ->live_caption_title_new_badge_;
  }

  void OnSodaProgress(int progress) {
    MediaDialogView::GetDialogViewForTesting()->OnSodaProgress(progress);
  }

  void OnSodaInstalled() {
    MediaDialogView::GetDialogViewForTesting()->OnSodaInstalled();
  }

 protected:
  std::unique_ptr<TestWebContentsPresentationManager> presentation_manager_;
  TestMediaRouter* media_router_ = nullptr;

 private:
  void ClickButton(views::Button* button) {
    base::RunLoop closure_loop;
    ui_test_utils::MoveMouseToCenterAndPress(
        button, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        closure_loop.QuitClosure());
    closure_loop.Run();
  }

  // Recursively tries to find a views::ImageButton for the given
  // MediaSessionAction. This operates under the assumption that
  // media_message_center::MediaNotificationViewImpl sets the tags of its action
  // buttons to the MediaSessionAction value.
  views::ImageButton* GetButtonForAction(views::View* view, int action) {
    if (views::IsViewClass<views::ImageButton>(view)) {
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
      const std::u16string& title) {
    for (const auto& notification_pair :
         MediaDialogView::GetDialogViewForTesting()
             ->GetNotificationsForTesting()) {
      const media_message_center::MediaNotificationViewImpl* view =
          notification_pair.second->view_for_testing();
      if (view->title_label_for_testing()->GetText() == title)
        return notification_pair.second;
    }
    return nullptr;
  }

  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription subscription_;

  DISALLOW_COPY_AND_ASSIGN(MediaDialogViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       ShowsMetadataAndControlsMedia) {
  // The toolbar icon should not start visible.
  EXPECT_FALSE(IsToolbarIconVisible());

  // Opening a page with media that hasn't played yet should not make the
  // toolbar icon visible.
  OpenTestURL();
  LayoutBrowser();
  EXPECT_FALSE(IsToolbarIconVisible());

  // Once playback starts, the icon should be visible, but the dialog should not
  // appear if it hasn't been clicked.
  StartPlayback();
  WaitForStart();
  WaitForVisibleToolbarIcon();
  EXPECT_TRUE(IsToolbarIconVisible());
  EXPECT_FALSE(IsDialogVisible());

  // At this point, the toolbar icon has been set visible. Layout the
  // browser to ensure it can be clicked.
  LayoutBrowser();

  // Clicking on the toolbar icon should open the dialog.
  ClickToolbarIcon();
  WaitForDialogOpened();
  EXPECT_TRUE(IsDialogVisible());

  // The dialog should contain the title and artist. These are taken from
  // video-with-metadata.html.
  WaitForDialogToContainText(u"Big Buck Bunny");
  WaitForDialogToContainText(u"Blender Foundation");

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

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       ShowsMetadataAndControlsMediaInRTL) {
  base::i18n::SetICUDefaultLocale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());

  // The toolbar icon should not start visible.
  EXPECT_FALSE(IsToolbarIconVisible());

  // Opening a page with media that hasn't played yet should not make the
  // toolbar icon visible.
  OpenTestURL();
  LayoutBrowser();
  EXPECT_FALSE(IsToolbarIconVisible());

  // Once playback starts, the icon should be visible, but the dialog should not
  // appear if it hasn't been clicked.
  StartPlayback();
  WaitForStart();
  WaitForVisibleToolbarIcon();
  EXPECT_TRUE(IsToolbarIconVisible());
  EXPECT_FALSE(IsDialogVisible());

  // At this point, the toolbar icon has been set visible. Layout the
  // browser to ensure it can be clicked.
  LayoutBrowser();

  // Clicking on the toolbar icon should open the dialog.
  ClickToolbarIcon();
  WaitForDialogOpened();
  EXPECT_TRUE(IsDialogVisible());

  // The view containing playback controls should not be mirrored.
  EXPECT_FALSE(MediaDialogView::GetDialogViewForTesting()
                   ->GetNotificationsForTesting()
                   .begin()
                   ->second->view_for_testing()
                   ->playback_button_container_for_testing()
                   ->GetMirrored());

  // The dialog should contain the title and artist. These are taken from
  // video-with-metadata.html.
  WaitForDialogToContainText(u"Big Buck Bunny");
  WaitForDialogToContainText(u"Blender Foundation");

  // Clicking on the pause button in the dialog should pause the media on the
  // page.
  ClickPauseButtonOnDialog();
  WaitForStop();

  // Clicking on the play button in the dialog should play the media on the
  // page.
  ClickPlayButtonOnDialog();
  WaitForStart();

  // In the RTL UI the picture in picture button should be to the left of the
  // playback control buttons.
  EXPECT_LT(
      GetButtonForAction(MediaSessionAction::kEnterPictureInPicture)
          ->GetMirroredX(),
      GetButtonForAction(MediaSessionAction::kPlay)->parent()->GetMirroredX());

  // In the RTL UI the focus order should be the same as it is in the LTR UI.
  // That is the play/pause button logically proceeds the picture in picture
  // button.
  EXPECT_TRUE(ViewFollowsInFocusChain(
      GetButtonForAction(MediaSessionAction::kPlay)->parent(),
      GetButtonForAction(MediaSessionAction::kEnterPictureInPicture)));

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
  WaitForDialogToContainText(u"Big Buck Bunny");
  WaitForDialogToContainText(u"Blender Foundation");
  WaitForDialogToContainText(u"Different Title");
  WaitForDialogToContainText(u"Another Artist");
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
  WaitForDialogToContainText(u"Big Buck Bunny");
  WaitForDialogToContainText(u"Different Title");

  // The second tab should be the active tab.
  EXPECT_EQ(second_web_contents, GetActiveWebContents());

  // Clicking the first notification should make the first tab active.
  ClickNotificationByTitle(u"Big Buck Bunny");
  EXPECT_EQ(first_web_contents, GetActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest, ShowsCastSession) {
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  const std::string route_description = "Casting: Big Buck Bunny";
  const std::string sink_name = "My Sink";
  media_router::MediaRoute route("id", media_router::MediaSource("source_id"),
                                 "sink_id", route_description, true, true);
  route.set_media_sink_name(sink_name);
  route.set_controller_type(media_router::RouteControllerType::kGeneric);
  media_router_->NotifyMediaRoutesChanged({route});
  base::RunLoop().RunUntilIdle();
  presentation_manager_->NotifyMediaRoutesChanged({route});

  WaitForVisibleToolbarIcon();
  ClickToolbarIcon();
  WaitForDialogOpened();
  WaitForDialogToContainText(
      base::UTF8ToUTF16(route_description + " \xC2\xB7 " + sink_name));
  WaitForNotificationCount(1);
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest, PictureInPicture) {
  // Open a tab and play media.
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  // Open the media dialog.
  WaitForVisibleToolbarIcon();
  ClickToolbarIcon();
  WaitForDialogOpened();
  EXPECT_TRUE(IsDialogVisible());

  ClickEnterPictureInPictureButtonOnDialog();
  WaitForEnterPictureInPicture();

  ClickExitPictureInPictureButtonOnDialog();
  WaitForExitPictureInPicture();
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       PictureInPictureButtonVisibility) {
  // Open a tab and play media.
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  // Open the media dialog.
  WaitForVisibleToolbarIcon();
  ClickToolbarIcon();
  WaitForDialogOpened();
  EXPECT_TRUE(IsDialogVisible());

  DisablePictureInPicture();
  WaitForPictureInPictureButtonVisibility(false);

  EnablePictureInPicture();
  WaitForPictureInPictureButtonVisibility(true);
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       PlayingSessionAlwaysDisplayFirst) {
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  content::WebContents* first_web_contents = GetActiveWebContents();

  OpenDifferentMetadataURLInNewTab();
  StartPlayback();
  WaitForStart();

  WaitForVisibleToolbarIcon();
  EXPECT_TRUE(IsToolbarIconVisible());

  ClickToolbarIcon();
  WaitForDialogOpened();
  EXPECT_TRUE(IsDialogVisible());

  // Pause the first session.
  ClickPauseButtonOnDialog();
  WaitForStop(first_web_contents);

  // Reopen dialog.
  ClickToolbarIcon();
  EXPECT_FALSE(IsDialogVisible());
  ClickToolbarIcon();
  WaitForDialogOpened();
  EXPECT_TRUE(IsDialogVisible());

  EXPECT_TRUE(IsPlayingSessionDisplayedFirst());
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest, LiveCaption) {
  // Open a tab and play media.
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  // Open the media dialog.
  WaitForVisibleToolbarIcon();
  ClickToolbarIcon();
  WaitForDialogOpened();
  EXPECT_TRUE(IsDialogVisible());

  // When media dialog opens and Live Caption is disabled, the New badge is
  // visible and the regular title is not visible.
  EXPECT_NE(GetLiveCaptionTitleNewBadgeLabel(), nullptr);
  EXPECT_TRUE(GetLiveCaptionTitleNewBadgeLabel()->GetVisible());
  EXPECT_FALSE(GetLiveCaptionTitleLabel()->GetVisible());

  ClickEnableLiveCaptionOnDialog();
  EXPECT_TRUE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  // The New Badge disappears when Live Caption is enabled. The regular title
  // appears.
  EXPECT_FALSE(GetLiveCaptionTitleNewBadgeLabel()->GetVisible());
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());

  ClickEnableLiveCaptionOnDialog();
  EXPECT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  // The New Badge doesn't reappear after Live Caption is disabled again.
  EXPECT_FALSE(GetLiveCaptionTitleNewBadgeLabel()->GetVisible());
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());

  // Close dialog and enable live caption preference. Reopen dialog.
  ClickToolbarIcon();
  EXPECT_FALSE(IsDialogVisible());
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                               true);
  ClickToolbarIcon();
  WaitForDialogOpened();
  EXPECT_TRUE(IsDialogVisible());
  // When media dialog opens and Live Caption is enabled, the New badge is not
  // created. The regular title is visible.
  EXPECT_EQ(GetLiveCaptionTitleNewBadgeLabel(), nullptr);
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());

  ClickEnableLiveCaptionOnDialog();
  EXPECT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  // The New badge is still not created. The regular title is still visible.
  EXPECT_EQ(GetLiveCaptionTitleNewBadgeLabel(), nullptr);
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest, LiveCaptionProgressUpdate) {
  // Open a tab and play media.
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  // Open the media dialog.
  WaitForVisibleToolbarIcon();
  ClickToolbarIcon();
  WaitForDialogOpened();
  EXPECT_TRUE(IsDialogVisible());

  EXPECT_EQ("Live Caption (English only)",
            base::UTF16ToUTF8(GetLiveCaptionTitleNewBadgeLabel()->GetText()));

  ClickEnableLiveCaptionOnDialog();
  OnSodaProgress(0);
  EXPECT_EQ("Downloading… 0%",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  OnSodaProgress(12);
  EXPECT_EQ("Downloading… 12%",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  OnSodaProgress(100);
  EXPECT_EQ("Downloading… 100%",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  OnSodaInstalled();
  EXPECT_EQ("Live Caption (English only)",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
