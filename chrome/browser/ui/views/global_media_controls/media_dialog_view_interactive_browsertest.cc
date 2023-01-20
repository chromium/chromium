// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_media_controls/media_toolbar_button_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_ui_for_test.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view_observer.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/global_media_controls/public/views/media_item_ui_list_view.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/media_message_center/media_notification_view_impl.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/soda/constants.h"
#include "content/public/browser/presentation_observer.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/media_start_stop_observer.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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

  MediaToolbarButtonWatcher(const MediaToolbarButtonWatcher&) = delete;
  MediaToolbarButtonWatcher& operator=(const MediaToolbarButtonWatcher&) =
      delete;

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

  void OnMediaButtonHidden() override {
    waiting_for_button_hidden_ = false;
    MaybeStopWaiting();
  }

  void OnMediaButtonEnabled() override {}
  void OnMediaButtonDisabled() override {}

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

  // Checks the title and artist of each item in the dialog to see if
  // |text| is contained anywhere in the dialog.
  bool DialogContainsText(const std::u16string& text) {
    for (const auto& item_pair :
         MediaDialogView::GetDialogViewForTesting()->GetItemsForTesting()) {
      const media_message_center::MediaNotificationViewImpl* view =
          item_pair.second->view_for_testing();
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
    const auto item_pair = MediaDialogView::GetDialogViewForTesting()
                               ->GetItemsForTesting()
                               .begin();
    const media_message_center::MediaNotificationViewImpl* view =
        item_pair->second->view_for_testing();

    return view->picture_in_picture_button_for_testing()->GetVisible() ==
           visible;
  }

  int GetItemCount() {
    return MediaDialogView::GetDialogViewForTesting()
        ->GetItemsForTesting()
        .size();
  }

  const raw_ptr<MediaToolbarButtonView> button_;
  std::unique_ptr<base::RunLoop> run_loop_;

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

class TestWebContentsPresentationManager
    : public media_router::WebContentsPresentationManager {
 public:
  void NotifyPresentationsChanged(bool has_presentation) {
    for (auto& observer : observers_) {
      observer.OnPresentationsChanged(has_presentation);
    }
  }

  void AddObserver(content::PresentationObserver* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(content::PresentationObserver* observer) override {
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
  base::ObserverList<content::PresentationObserver> observers_;
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

  void NotifyMediaRoutesChanged(
      const std::vector<media_router::MediaRoute>& routes) {
    for (auto& observer : routes_observers_)
      observer.OnRoutesUpdated(routes);
  }

 private:
  std::unique_ptr<media_router::LoggerImpl> logger_;
};

}  // anonymous namespace

class MediaDialogViewBrowserTest : public InProcessBrowserTest {
 public:
  MediaDialogViewBrowserTest() {
    feature_list_.InitWithFeatures(
        {media::kGlobalMediaControls, media::kLiveCaption,
         feature_engagement::kIPHLiveCaptionFeature,
         media::kLiveCaptionMultiLanguage},
        {});
  }

  MediaDialogViewBrowserTest(const MediaDialogViewBrowserTest&) = delete;
  MediaDialogViewBrowserTest& operator=(const MediaDialogViewBrowserTest&) =
      delete;

  ~MediaDialogViewBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void SetUp() override {
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

  void OpenTestURL() {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("media/session")),
        base::FilePath(FILE_PATH_LITERAL("video-with-metadata.html")));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
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
    GetActiveWebContents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
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
    GetActiveWebContents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"disablePictureInPicture()", base::NullCallback());
  }

  void EnablePictureInPicture() {
    GetActiveWebContents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"enablePictureInPicture()", base::NullCallback());
  }

  void WaitForEnterPictureInPicture() {
    if (GetActiveWebContents()->HasPictureInPictureVideo())
      return;

    content::MediaStartStopObserver observer(
        GetActiveWebContents(),
        content::MediaStartStopObserver::Type::kEnterPictureInPicture);
    observer.Wait();
  }

  void WaitForExitPictureInPicture() {
    if (!GetActiveWebContents()->HasPictureInPictureVideo())
      return;

    content::MediaStartStopObserver observer(
        GetActiveWebContents(),
        content::MediaStartStopObserver::Type::kExitPictureInPicture);
    observer.Wait();
  }

  void ClickPauseButtonOnDialog() {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(MediaDialogView::IsShowing());
    ui_test_utils::ClickOnView(GetButtonForAction(MediaSessionAction::kPause));
  }

  void ClickPlayButtonOnDialog() {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(MediaDialogView::IsShowing());
    ui_test_utils::ClickOnView(GetButtonForAction(MediaSessionAction::kPlay));
  }

  void ClickEnterPictureInPictureButtonOnDialog() {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(MediaDialogView::IsShowing());
    ui_test_utils::ClickOnView(
        GetButtonForAction(MediaSessionAction::kEnterPictureInPicture));
  }

  void ClickExitPictureInPictureButtonOnDialog() {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(MediaDialogView::IsShowing());
    ui_test_utils::ClickOnView(
        GetButtonForAction(MediaSessionAction::kExitPictureInPicture));
  }

  void ClickEnableLiveCaptionOnDialog() {
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(MediaDialogView::IsShowing());
    views::Button* live_caption_button = static_cast<views::Button*>(
        MediaDialogView::GetDialogViewForTesting()->live_caption_button_);
    ui_test_utils::ClickOnView(live_caption_button);
  }

  void ClickItemByTitle(const std::u16string& title) {
    ASSERT_TRUE(MediaDialogView::IsShowing());
    global_media_controls::MediaItemUIView* item = GetItemByTitle(title);
    ASSERT_NE(nullptr, item);
    ui_test_utils::ClickOnView(item);
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
      global_media_controls::MediaItemUIView* item =
          static_cast<global_media_controls::MediaItemUIView*>(view);

      if (seen_paused && item->is_playing_for_testing())
        return false;

      if (!seen_paused && !item->is_playing_for_testing())
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

  void OnSodaProgress(int progress) {
    speech::SodaInstaller::GetInstance()->NotifySodaProgressForTesting(
        progress);
  }

  void OnSodaInstalled() {
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting();
  }

  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) {
    speech::SodaInstaller::GetInstance()->NotifySodaErrorForTesting(
        language_code, error_code);
  }

  void OnSodaLanguagePackInstalled() {
    speech::SodaInstaller::GetInstance()->NotifySodaInstalledForTesting(
        speech::LanguageCode::kEnUs);
  }

 protected:
  std::unique_ptr<TestWebContentsPresentationManager> presentation_manager_;
  raw_ptr<TestMediaRouter, DanglingUntriaged> media_router_ = nullptr;
  MediaDialogUiForTest ui_{base::BindRepeating(&InProcessBrowserTest::browser,
                                               base::Unretained(this))};

 private:
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

  // Finds a global_media_controls::MediaItemUIView by title.
  global_media_controls::MediaItemUIView* GetItemByTitle(
      const std::u16string& title) {
    for (const auto& item_pair :
         MediaDialogView::GetDialogViewForTesting()->GetItemsForTesting()) {
      const media_message_center::MediaNotificationViewImpl* view =
          item_pair.second->view_for_testing();
      if (view->title_label_for_testing()->GetText() == title)
        return item_pair.second;
    }
    return nullptr;
  }

  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription subscription_;
};

// This test was first disabled on BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
// for https://crbug.com/1222873.
// Then got disabled on all platforms for https://crbug.com/1225531.
IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       DISABLED_ShowsMetadataAndControlsMedia) {
  // The toolbar icon should not start visible.
  EXPECT_FALSE(ui_.IsToolbarIconVisible());

  // Opening a page with media that hasn't played yet should not make the
  // toolbar icon visible.
  OpenTestURL();
  ui_.LayoutBrowserIfNecessary();
  EXPECT_FALSE(ui_.IsToolbarIconVisible());

  // Once playback starts, the icon should be visible, but the dialog should not
  // appear if it hasn't been clicked.
  StartPlayback();
  WaitForStart();
  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  EXPECT_TRUE(ui_.IsToolbarIconVisible());
  EXPECT_FALSE(ui_.IsDialogVisible());

  // At this point, the toolbar icon has been set visible. Layout the
  // browser to ensure it can be clicked.
  ui_.LayoutBrowserIfNecessary();

  // Clicking on the toolbar icon should open the dialog.
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  // The dialog should contain the title and artist. These are taken from
  // video-with-metadata.html.
  ui_.WaitForDialogToContainText(u"Big Buck Bunny");
  ui_.WaitForDialogToContainText(u"Blender Foundation");

  // Clicking on the pause button in the dialog should pause the media on the
  // page.
  ClickPauseButtonOnDialog();
  WaitForStop();

  // Clicking on the play button in the dialog should play the media on the
  // page.
  ClickPlayButtonOnDialog();
  WaitForStart();

  // Clicking on the toolbar icon again should hide the dialog.
  EXPECT_TRUE(ui_.IsDialogVisible());
  ui_.ClickToolbarIcon();
  EXPECT_FALSE(ui_.IsDialogVisible());
}

// TODO(crbug.com/1225531, crbug.com/1222873): Flaky.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_ShowsMetadataAndControlsMediaInRTL \
  DISABLED_ShowsMetadataAndControlsMediaInRTL
#else
#define MAYBE_ShowsMetadataAndControlsMediaInRTL \
  ShowsMetadataAndControlsMediaInRTL
#endif
IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       MAYBE_ShowsMetadataAndControlsMediaInRTL) {
  base::i18n::SetICUDefaultLocale("ar");
  ASSERT_TRUE(base::i18n::IsRTL());

  // The toolbar icon should not start visible.
  EXPECT_FALSE(ui_.IsToolbarIconVisible());

  // Opening a page with media that hasn't played yet should not make the
  // toolbar icon visible.
  OpenTestURL();
  ui_.LayoutBrowserIfNecessary();
  EXPECT_FALSE(ui_.IsToolbarIconVisible());

  // Once playback starts, the icon should be visible, but the dialog should not
  // appear if it hasn't been clicked.
  StartPlayback();
  WaitForStart();
  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  EXPECT_TRUE(ui_.IsToolbarIconVisible());
  EXPECT_FALSE(ui_.IsDialogVisible());

  // At this point, the toolbar icon has been set visible. Layout the
  // browser to ensure it can be clicked.
  ui_.LayoutBrowserIfNecessary();

  // Clicking on the toolbar icon should open the dialog.
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  // The view containing playback controls should not be mirrored.
  EXPECT_FALSE(MediaDialogView::GetDialogViewForTesting()
                   ->GetItemsForTesting()
                   .begin()
                   ->second->view_for_testing()
                   ->playback_button_container_for_testing()
                   ->GetMirrored());

  // The dialog should contain the title and artist. These are taken from
  // video-with-metadata.html.
  ui_.WaitForDialogToContainText(u"Big Buck Bunny");
  ui_.WaitForDialogToContainText(u"Blender Foundation");

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
  EXPECT_TRUE(ui_.IsDialogVisible());
  ui_.ClickToolbarIcon();
  EXPECT_FALSE(ui_.IsDialogVisible());
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
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  // The dialog should show both media sessions.
  ui_.WaitForDialogToContainText(u"Big Buck Bunny");
  ui_.WaitForDialogToContainText(u"Blender Foundation");
  ui_.WaitForDialogToContainText(u"Different Title");
  ui_.WaitForDialogToContainText(u"Another Artist");
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       ClickingOnItemGoesBackToTab) {
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
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  // Wait for the dialog to be populated.
  ui_.WaitForDialogToContainText(u"Big Buck Bunny");
  ui_.WaitForDialogToContainText(u"Different Title");

  // The second tab should be the active tab.
  EXPECT_EQ(second_web_contents, GetActiveWebContents());

  // Clicking the first item should make the first tab active.
  ClickItemByTitle(u"Big Buck Bunny");

  // Allow the MediaSessionNotificationItem to flush its message to the
  // MediaSessionImpl. There isn't currently a clean way for us to access the
  // MediaSessionNotificationItem directly to force it to flush, so we use this
  // non-ideal |RunUntilIdle()| call instead.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(first_web_contents, GetActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest, ShowsCastSession) {
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  const std::string route_description = "Casting: Big Buck Bunny";
  const std::string sink_name = "My Sink";
  media_router::MediaRoute route("id", media_router::MediaSource("source_id"),
                                 "sink_id", route_description, true);
  route.set_media_sink_name(sink_name);
  route.set_controller_type(media_router::RouteControllerType::kGeneric);
  media_router_->NotifyMediaRoutesChanged({route});
  base::RunLoop().RunUntilIdle();
  presentation_manager_->NotifyPresentationsChanged(true);

  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  ui_.WaitForDialogToContainText(
      base::UTF8ToUTF16(route_description + " \xC2\xB7 " + sink_name));
  ui_.WaitForItemCount(1);
}

#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
// https://crbug.com/1224071
#define MAYBE_PictureInPicture DISABLED_PictureInPicture
#else
#define MAYBE_PictureInPicture PictureInPicture
#endif
// Test is flaky crbug.com/1213256.
IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest, MAYBE_PictureInPicture) {
  // Open a tab and play media.
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  // Open the media dialog.
  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

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
  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  DisablePictureInPicture();
  ui_.WaitForPictureInPictureButtonVisibility(false);

  EnablePictureInPicture();
  ui_.WaitForPictureInPictureButtonVisibility(true);
}

// Flaky on multiple platforms (crbug.com/1218003,crbug.com/1383904).
IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       DISABLED_PlayingSessionAlwaysDisplayFirst) {
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  content::WebContents* first_web_contents = GetActiveWebContents();

  OpenDifferentMetadataURLInNewTab();
  StartPlayback();
  WaitForStart();

  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  EXPECT_TRUE(ui_.IsToolbarIconVisible());

  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  // Pause the first session.
  ClickPauseButtonOnDialog();
  WaitForStop(first_web_contents);

  // Reopen dialog.
  ui_.ClickToolbarIcon();
  EXPECT_FALSE(ui_.IsDialogVisible());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  EXPECT_TRUE(IsPlayingSessionDisplayedFirst());
}

// TODO(crbug.com/1225531, crbug.com/1222873, crbug.com/1271131): Flaky.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_LiveCaption DISABLED_LiveCaption
#else
#define MAYBE_LiveCaption LiveCaption
#endif
IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest, MAYBE_LiveCaption) {
  // Open a tab and play media.
  OpenTestURL();
  StartPlayback();
  WaitForStart();
  speech::SodaInstaller::GetInstance()->NeverDownloadSodaForTesting();

  // Open the media dialog.
  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  // The Live Caption title should appear.
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ("Live Caption",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  // Click the Live Caption toggle to toggle it on.
  ClickEnableLiveCaptionOnDialog();
  EXPECT_TRUE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ("Live Caption - English",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  // Click the Live Caption toggle again to toggle it off.
  ClickEnableLiveCaptionOnDialog();
  EXPECT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ("Live Caption",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  // Close dialog and enable live caption preference. Reopen dialog.
  ui_.ClickToolbarIcon();
  EXPECT_FALSE(ui_.IsDialogVisible());
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kLiveCaptionEnabled,
                                               true);
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ("Live Caption - English",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  // Click the Live Caption toggle to toggle it off.
  ClickEnableLiveCaptionOnDialog();
  EXPECT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ("Live Caption",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  OnSodaInstallError(speech::LanguageCode::kNone,
                     speech::SodaInstaller::ErrorCode::kNeedsReboot);
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_DOWNLOAD_ERROR_REBOOT_REQUIRED),
      GetLiveCaptionTitleLabel()->GetText());

  OnSodaInstallError(speech::LanguageCode::kNone,
                     speech::SodaInstaller::ErrorCode::kUnspecifiedError);
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_GLOBAL_MEDIA_CONTROLS_LIVE_CAPTION_DOWNLOAD_ERROR),
            GetLiveCaptionTitleLabel()->GetText());
}

#if BUILDFLAG(IS_MAC)
// https://crbug.com/1222873
// Flaky on all Mac bots: https://crbug.com/1274967
#define MAYBE_LiveCaptionProgressUpdate DISABLED_LiveCaptionProgressUpdate
#else
#define MAYBE_LiveCaptionProgressUpdate LiveCaptionProgressUpdate
#endif
IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       MAYBE_LiveCaptionProgressUpdate) {
  speech::SodaInstaller::GetInstance()->NeverDownloadSodaForTesting();
  // Open a tab and play media.
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  // Open the media dialog.
  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  EXPECT_EQ("Live Caption",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  ClickEnableLiveCaptionOnDialog();
  OnSodaProgress(0);
  EXPECT_EQ("Downloading… 0%",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  OnSodaProgress(12);
  EXPECT_EQ("Downloading… 12%",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  // Click the Live Caption toggle again to toggle it off.
  ClickEnableLiveCaptionOnDialog();
  EXPECT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ("Downloading… 12%",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  // Download progress should continue to update when the Live Caption toggle is
  // off.
  OnSodaProgress(42);
  EXPECT_EQ("Downloading… 42%",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  // Click the Live Caption toggle again to toggle it on.
  ClickEnableLiveCaptionOnDialog();
  EXPECT_TRUE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ("Downloading… 42%",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  OnSodaProgress(100);
  EXPECT_EQ("Downloading… 100%",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  OnSodaLanguagePackInstalled();
  EXPECT_EQ("Downloading… 100%",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  OnSodaInstalled();
  EXPECT_EQ("Live Caption - English",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));
}

// TODO(crbug.com/1225531, crbug.com/1222873): Flaky.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#define MAYBE_LiveCaptionShowLanguage DISABLED_LiveCaptionShowLanguage
#else
#define MAYBE_LiveCaptionShowLanguage LiveCaptionShowLanguage
#endif
IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       MAYBE_LiveCaptionShowLanguage) {
  // Open a tab and play media.
  OpenTestURL();
  StartPlayback();
  WaitForStart();
  speech::SodaInstaller::GetInstance()->NeverDownloadSodaForTesting();

  // Open the media dialog.
  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  // Live Caption is disabled, so the title should not show the language.
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ("Live Caption",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  // When Live Caption is enabled, the title should show the language.
  ClickEnableLiveCaptionOnDialog();
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ("Live Caption - English",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  // Close dialog and change live caption language. Reopen dialog.
  ui_.ClickToolbarIcon();
  EXPECT_FALSE(ui_.IsDialogVisible());
  browser()->profile()->GetPrefs()->SetString(prefs::kLiveCaptionLanguageCode,
                                              "de-DE");
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  // Live Caption is enabled so the title should show the new language.
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ("Live Caption - German",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));

  // When Live Caption is disabled, the title should not show the language.
  ClickEnableLiveCaptionOnDialog();
  EXPECT_TRUE(GetLiveCaptionTitleLabel()->GetVisible());
  EXPECT_EQ("Live Caption",
            base::UTF16ToUTF8(GetLiveCaptionTitleLabel()->GetText()));
}

class MediaDialogViewWithBackForwardCacheBrowserTest
    : public MediaDialogViewBrowserTest {
 protected:
  MediaDialogViewWithBackForwardCacheBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::map<std::string, std::string> params;
#if BUILDFLAG(IS_ANDROID)
    params["process_binding_strength"] = "NORMAL";
#endif
    enabled_features.emplace_back(features::kBackForwardCache, params);
    enabled_features.emplace_back(
        features::kBackForwardCacheMediaSessionService,
        std::map<std::string, std::string>{});

    std::vector<base::test::FeatureRef> disabled_features = {
        features::kBackForwardCacheMemoryControls,
    };

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    host_resolver()->AddRule("*", "127.0.0.1");

    MediaDialogViewBrowserTest::SetUpOnMainThread();
  }

  content::RenderFrameHost* GetPrimaryMainFrame() {
    return GetActiveWebContents()->GetPrimaryMainFrame();
  }

 protected:
  base::FilePath GetTestDataDirectory() {
    base::FilePath test_file_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_directory);
    return test_file_directory;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MediaDialogViewWithBackForwardCacheBrowserTest,
                       PlayAndCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL(
      "a.test", "/media/session/video-with-metadata.html"));
  GURL url2(embedded_test_server()->GetURL("b.test", "/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  content::RenderFrameHost* rfh = GetPrimaryMainFrame();

  StartPlayback();
  WaitForStart();

  // Open the media dialog.
  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  // Navigate to another page. The original page is cached.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  EXPECT_EQ(content::RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh->GetLifecycleState());
  EXPECT_TRUE(ui_.WaitForToolbarIconHidden());
  EXPECT_FALSE(ui_.IsDialogVisible());

  // Go back to the original page. The original page is restored from the cache.
  GetActiveWebContents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(GetActiveWebContents()));
  EXPECT_NE(content::RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh->GetLifecycleState());
  EXPECT_FALSE(ui_.IsDialogVisible());

  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewWithBackForwardCacheBrowserTest,
                       DISABLED_PauseAndCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL(
      "a.test", "/media/session/video-with-metadata.html"));
  GURL url2(embedded_test_server()->GetURL("b.test", "/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  content::RenderFrameHost* rfh = GetPrimaryMainFrame();

  StartPlayback();
  WaitForStart();

  // Open the media dialog.
  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  // Pause the media.
  ClickPauseButtonOnDialog();
  WaitForStop();
  EXPECT_TRUE(ui_.IsDialogVisible());

  // Close the dialog.
  ui_.ClickToolbarIcon();
  EXPECT_FALSE(ui_.IsDialogVisible());

  // Navigate to another page. The original page is cached.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  EXPECT_EQ(content::RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh->GetLifecycleState());
  EXPECT_TRUE(ui_.WaitForToolbarIconHidden());
  EXPECT_FALSE(ui_.IsDialogVisible());

  // Go back to the original page. The original page is restored from the cache.
  GetActiveWebContents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(GetActiveWebContents()));
  EXPECT_NE(content::RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh->GetLifecycleState());
  EXPECT_FALSE(ui_.IsDialogVisible());

  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewWithBackForwardCacheBrowserTest,
                       DISABLED_CacheTwiceAndGoBack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1(embedded_test_server()->GetURL(
      "a.test", "/media/session/video-with-metadata.html"));
  GURL url2(embedded_test_server()->GetURL(
      "b.test", "/media/session/video-with-metadata.html"));
  GURL url3(embedded_test_server()->GetURL(
      "c.test", "/media/session/video-with-metadata.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  content::RenderFrameHost* rfh1 = GetPrimaryMainFrame();

  StartPlayback();
  WaitForStart();

  // Open the media dialog.
  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  EXPECT_TRUE(ui_.IsDialogVisible());

  // Navigate to another page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  EXPECT_EQ(content::RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh1->GetLifecycleState());
  EXPECT_TRUE(ui_.WaitForToolbarIconHidden());
  EXPECT_FALSE(ui_.IsDialogVisible());
  content::RenderFrameHost* rfh2 = GetPrimaryMainFrame();

  StartPlayback();
  WaitForStart();

  // Navigate to yet another page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url3));
  EXPECT_EQ(content::RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh2->GetLifecycleState());
  EXPECT_TRUE(ui_.WaitForToolbarIconHidden());
  EXPECT_FALSE(ui_.IsDialogVisible());

  // Go back.
  GetActiveWebContents()->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(GetActiveWebContents()));
  EXPECT_NE(content::RenderFrameHost::LifecycleState::kInBackForwardCache,
            rfh2->GetLifecycleState());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
