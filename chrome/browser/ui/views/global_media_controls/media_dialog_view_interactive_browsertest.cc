// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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
#include "components/global_media_controls/public/views/media_item_ui_updated_view.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/media_router/browser/media_routes_observer.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/soda/constants.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/presentation_observer.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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

class TestWebContentsPresentationManager
    : public media_router::WebContentsPresentationManager {
 public:
  void NotifyPresentationsChanged(bool has_presentation) {
    observers_.Notify(&content::PresentationObserver::OnPresentationsChanged,
                      has_presentation);
  }

  void AddObserver(content::PresentationObserver* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(content::PresentationObserver* observer) override {
    observers_.RemoveObserver(observer);
  }

  MOCK_METHOD(bool, HasDefaultPresentationRequest, (), (const, override));
  MOCK_METHOD(const content::PresentationRequest&,
              GetDefaultPresentationRequest,
              (),
              (const, override));
  MOCK_METHOD(std::vector<media_router::MediaRoute>,
              GetMediaRoutes,
              (),
              (override));

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
    routes_observers_.Notify(
        &media_router::MediaRoutesObserver::OnRoutesUpdated, routes);
  }

 private:
  std::unique_ptr<media_router::LoggerImpl> logger_;
};

}  // anonymous namespace

class MediaDialogViewBrowserTest : public InProcessBrowserTest {
 public:
  MediaDialogViewBrowserTest() {
    feature_list_.InitWithFeatures(
        {media::kGlobalMediaControls,
         feature_engagement::kIPHLiveCaptionFeature,
         media::kFeatureManagementLiveTranslateCrOS,
         media::kLiveCaptionMultiLanguage, media::kLiveTranslate,
         media::kGlobalMediaControlsUpdatedUI},
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
    ON_CALL(*media_router_, RegisterMediaSinksObserver)
        .WillByDefault(testing::Return(true));
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
        u"play()", base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
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
        u"disablePictureInPicture()", base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
  }

  void EnablePictureInPicture() {
    GetActiveWebContents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"enablePictureInPicture()", base::NullCallback(),
        content::ISOLATED_WORLD_ID_GLOBAL);
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
    base::RunLoop run_loop;
    PrefChangeRegistrar change_observer;
    change_observer.Init(browser()->profile()->GetPrefs());
    change_observer.Add(prefs::kLiveCaptionEnabled, run_loop.QuitClosure());

    ASSERT_TRUE(MediaDialogView::IsShowing());
    views::Button* live_caption_button = static_cast<views::Button*>(
        MediaDialogView::GetDialogViewForTesting()->live_caption_button_);
    ui_test_utils::ClickOnView(live_caption_button);
    run_loop.Run();
  }

  void ClickEnableLiveTranslateOnDialog() {
    base::RunLoop().RunUntilIdle();
    base::RunLoop run_loop;
    PrefChangeRegistrar change_observer;
    change_observer.Init(browser()->profile()->GetPrefs());
    change_observer.Add(prefs::kLiveTranslateEnabled, run_loop.QuitClosure());

    ASSERT_TRUE(MediaDialogView::IsShowing());
    views::Button* live_translate_button = static_cast<views::Button*>(
        MediaDialogView::GetDialogViewForTesting()->live_translate_button_);
    ui_test_utils::ClickOnView(live_translate_button);
    run_loop.Run();
  }

  void ClickMediaViewByTitle(const std::u16string& title) {
    ASSERT_TRUE(MediaDialogView::IsShowing());
    global_media_controls::MediaItemUIUpdatedView* view = GetViewByTitle(title);
    ASSERT_NE(nullptr, view);
    ui_test_utils::ClickOnView(view);
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
      global_media_controls::MediaItemUIUpdatedView* media_view =
          static_cast<global_media_controls::MediaItemUIUpdatedView*>(view);
      if (seen_paused && media_view->GetMediaActionButtonForTesting(
                             MediaSessionAction::kPlay)) {
        return false;
      }
      if (!seen_paused && media_view->GetMediaActionButtonForTesting(
                              MediaSessionAction::kPause)) {
        seen_paused = true;
      }
    }
    return true;
  }

  views::ImageButton* GetButtonForAction(MediaSessionAction action) {
    for (views::View* view : MediaDialogView::GetDialogViewForTesting()
                                 ->GetListViewForTesting()
                                 ->contents()
                                 ->children()) {
      global_media_controls::MediaItemUIUpdatedView* media_view =
          static_cast<global_media_controls::MediaItemUIUpdatedView*>(view);
      if (media_view->GetMediaActionButtonForTesting(action)) {
        return media_view->GetMediaActionButtonForTesting(action);
      }
    }
    return nullptr;
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

  views::Label* GetLiveTranslateTitleLabel() {
    return MediaDialogView::GetDialogViewForTesting()->live_translate_title_;
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
  // Finds a global_media_controls::MediaItemUIUpdatedView by title.
  global_media_controls::MediaItemUIUpdatedView* GetViewByTitle(
      const std::u16string& title) {
    for (const auto& item_pair : MediaDialogView::GetDialogViewForTesting()
                                     ->GetUpdatedItemsForTesting()) {
      if (item_pair.second->GetTitleLabelForTesting()->GetText() == title) {
        return item_pair.second;
      }
    }
    return nullptr;
  }

  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       ShowsMetadataAndControlsMedia) {
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

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest,
                       ShowsMetadataAndControlsMediaInRTL) {
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

  // The play pause button should not be mirrored.
  EXPECT_FALSE(GetButtonForAction(MediaSessionAction::kPause)
                   ->GetFlipCanvasOnPaintForRTLUI());

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

  // In the RTL UI the forward button should be to the left of the replay
  // button.
  EXPECT_LT(
      GetButtonForAction(MediaSessionAction::kSeekForward)->GetMirroredX(),
      GetButtonForAction(MediaSessionAction::kSeekBackward)->GetMirroredX());

  // In the RTL UI the focus order should be the same as it is in the LTR UI.
  // That is the replay button logically proceeds the forward button.
  EXPECT_TRUE(ViewFollowsInFocusChain(
      GetButtonForAction(MediaSessionAction::kSeekBackward),
      GetButtonForAction(MediaSessionAction::kSeekForward)));

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

  // Clicking the first media view should make the first tab active.
  ClickMediaViewByTitle(u"Big Buck Bunny");

  // Allow the MediaSessionNotificationItem to flush its message to the
  // MediaSessionImpl. There isn't currently a clean way for us to access the
  // MediaSessionNotificationItem directly to force it to flush, so we use this
  // non-ideal |RunUntilIdle()| call instead.  This guarantees that the click
  // is processed by the item.  We then flush the media session separately, to
  // be sure that any calls on it by the notification item have been processed.
  base::RunLoop().RunUntilIdle();
  content::MediaSession::FlushObserversForTesting(first_web_contents);

  EXPECT_EQ(first_web_contents, GetActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest, ShowsCastSession) {
  OpenTestURL();
  StartPlayback();
  WaitForStart();

  const std::string route_description = "Casting: Big Buck Bunny";
  media_router::MediaRoute route("id", media_router::MediaSource("cast:123456"),
                                 "sink_id", route_description, true);
  route.set_media_sink_name("My Sink");
  route.set_controller_type(media_router::RouteControllerType::kGeneric);
  media_router_->NotifyMediaRoutesChanged({route});
  base::RunLoop().RunUntilIdle();
  presentation_manager_->NotifyPresentationsChanged(true);

  EXPECT_TRUE(ui_.WaitForToolbarIconShown());
  ui_.ClickToolbarIcon();
  EXPECT_TRUE(ui_.WaitForDialogOpened());
  ui_.WaitForDialogToContainText(base::UTF8ToUTF16(route_description));
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

// TODO(crbug.com/40898509): Live captioning not supported on Arm64 Windows.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64) || BUILDFLAG(IS_MAC)
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

#if (BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)) || BUILDFLAG(IS_MAC)
// https://crbug.com/1222873
// Flaky on all Mac bots: https://crbug.com/1274967
// TODO(crbug.com/40898509): Renable on WinArm64 when live captioning is
// enabled.
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
// TODO(crbug.com/40898509): Renable on WinArm64 when live captioning is
// enabled.
#if (BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)) || BUILDFLAG(IS_MAC)
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

IN_PROC_BROWSER_TEST_F(MediaDialogViewBrowserTest, LiveTranslate) {
  // Live captioning is not currently supported on Win Arm64.
  if (!captions::IsLiveCaptionFeatureSupported()) {
    GTEST_SKIP() << "Live caption feature not supported";
  }
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

  // Click the Live Caption toggle to toggle it on.
  ClickEnableLiveCaptionOnDialog();
  EXPECT_TRUE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));

  // The Live Translate title should appear.
  EXPECT_TRUE(GetLiveTranslateTitleLabel()->GetVisible());
  EXPECT_EQ("Live Translate",
            base::UTF16ToUTF8(GetLiveTranslateTitleLabel()->GetText()));

  // Click the Live Translate toggle to toggle it on.
  ClickEnableLiveTranslateOnDialog();
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kLiveTranslateEnabled));

  // Click the Live Caption toggle to toggle it off, which does not toggle off
  // Translate.
  ClickEnableLiveCaptionOnDialog();
  EXPECT_FALSE(
      browser()->profile()->GetPrefs()->GetBoolean(prefs::kLiveCaptionEnabled));
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kLiveTranslateEnabled));
}

class MediaDialogViewWithBackForwardCacheBrowserTest
    : public MediaDialogViewBrowserTest {
 protected:
  MediaDialogViewWithBackForwardCacheBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetBasicBackForwardCacheFeatureForTesting({
#if BUILDFLAG(IS_ANDROID)
          {features::kBackForwardCache,
           {
             { "process_binding_strength",
               "NORMAL" }
           }},
#endif
          {
            features::kBackForwardCacheMediaSessionService, {
              {}
            }
          }
        }),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
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

  // The restored page is paused, and we do not expect the toolbar icon to be
  // present until the playback restarts.
  EXPECT_TRUE(ui_.WaitForToolbarIconHidden());
  StartPlayback();
  WaitForStart();
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
