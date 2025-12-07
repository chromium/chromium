// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
#define CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/common/buildflags.h"
#include "components/user_education/common/new_badge/new_badge_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#endif  //  !BUILDFLAG(IS_ANDROID)

class LocationBarTesting;
class OmniboxView;

namespace qrcode_generator {
class QRCodeGeneratorBubbleView;
}  // namespace qrcode_generator

namespace send_tab_to_self {
class SendTabToSelfBubbleView;
}  // namespace send_tab_to_self

namespace sharing_hub {
class SharingHubBubbleView;
}  // namespace sharing_hub

// WARNING WARNING WARNING WARNING
// Do not use this class. See docs/chrome_browser_design_principles.md for
// details.  Either write a browser test which provides both a "class Browser"
// and a "class BrowserView" (a subclass of "class BrowserWindow") or a unit
// test which should require neither.
//
// An implementation of BrowserWindow used for testing. TestBrowserWindow only
// contains a valid LocationBar, all other getters return NULL.
// However, some of them can be preset to a specific value.
// See BrowserWithTestWindowTest for an example of using this class.
class TestBrowserWindow : public BrowserWindow, public BrowserListObserver {
 public:
  TestBrowserWindow();
  TestBrowserWindow(const TestBrowserWindow&) = delete;
  TestBrowserWindow& operator=(const TestBrowserWindow&) = delete;
  ~TestBrowserWindow() override;

  // BrowserWindow:
  void Show() override {}
  void ShowInactive() override {}
  void Hide() override {}
  bool IsVisible() const override;
  void SetBounds(const gfx::Rect& bounds) override {}
  void Close() override;
  void Activate() override {}
  void Deactivate() override {}
  bool IsActive() const override;
  void FlashFrame(bool flash) override {}
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override {}
  gfx::NativeWindow GetNativeWindow() const override;
  bool IsOnCurrentWorkspace() const override;
  bool IsVisibleOnScreen() const override;
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override;
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const override;
  ui::NativeTheme* GetNativeTheme() override;
  const ui::ThemeProvider* GetThemeProvider() const override;
  const ui::ColorProvider* GetColorProvider() const override;
  int GetTopControlsHeight() const override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  std::vector<StatusBubble*> GetStatusBubbles() override;
  void UpdateTitleBar() override {}
  void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) override {}
  void TemporarilyShowBookmarkBar(base::TimeDelta duration) override {}
  void UpdateDevTools(content::WebContents* inspected_web_contents) override {}
  bool CanDockDevTools() const override;
  void UpdateLoadingAnimations(bool is_visible) override {}
  void SetStarredState(bool is_starred) override {}
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents,
                          int index,
                          int reason) override {}
  void OnTabDetached(content::WebContents* contents, bool was_active) override {
  }
  void ZoomChangedForActiveTab(bool can_show_bubble) override {}
  gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  gfx::Size GetContentsSize() const override;
  void SetContentsSize(const gfx::Size& size) override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  void Maximize() override {}
  void Minimize() override {}
  void Restore() override {}
  void OnWebApiWindowResizableChanged() override {}
  bool GetCanResize() override;
  ui::mojom::WindowShowState GetWindowShowState() const override;
  bool ShouldHideUIForFullscreen() const override;
  bool IsFullscreen() const override;
  bool IsFullscreenBubbleVisible() const override;
  bool IsForceFullscreen() const override;
  void SetForceFullscreen(bool force_fullscreen) override {}
  LocationBar* GetLocationBar() const override;
  void UpdatePageActionIcon(PageActionIconType type) override {}
  autofill::AutofillBubbleHandler* GetAutofillBubbleHandler() override;
  void ExecutePageActionIconForTesting(PageActionIconType type) override {}
  void SetFocusToLocationBar(bool is_user_initiated) override {}
  void UpdateReloadStopState(bool is_loading, bool force) override {}
  void UpdateToolbar(content::WebContents* contents) override {}
  bool UpdateToolbarSecurityState() override;
  void UpdateCustomTabBarVisibility(bool visible, bool animate) override {}
  void SetDevToolsScrimVisibility(bool visible) override {}
  void ResetToolbarTabState(content::WebContents* contents) override {}
  void FocusToolbar() override {}
  ExtensionsContainer* GetExtensionsContainer() override;
  void ToolbarSizeChanged(bool is_animating) override {}
  void TabDraggingStatusChanged(bool is_dragging) override {}
  void LinkOpeningFromGesture(WindowOpenDisposition disposition) override {}
  void FocusAppMenu() override {}
  void FocusBookmarksToolbar() override {}
  void FocusInactivePopupForAccessibility() override {}
  void RotatePaneFocus(bool forwards) override {}
  void FocusWebContentsPane() override {}
  void ShowAppMenu() override {}
  void PreHandleDragUpdate(const content::DropData& drop_data,
                           const gfx::PointF& point) override {}

  void PreHandleDragExit() override {}
  void HandleDragEnded() override {}
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  bool IsBookmarkBarVisible() const override;
  bool IsBookmarkBarAnimating() const override;
  bool IsTabStripEditable() const override;
  void SetTabStripNotEditableForTesting() override;
  void SetIsTabStripEditable(bool is_editable);
  bool IsToolbarVisible() const override;
  bool IsLocationBarVisible() const override;
  bool IsToolbarShowing() const override;
  bool IsBorderlessModeEnabled() const override;
  void ShowChromeLabs() override {}
  BrowserView* AsBrowserView() override;
  SharingDialog* ShowSharingDialog(content::WebContents* contents,
                                   SharingDialogData data) override;
  void ShowUpdateChromeDialog() override {}
  void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) override {}
  qrcode_generator::QRCodeGeneratorBubbleView* ShowQRCodeGeneratorBubble(
      content::WebContents* contents,
      const GURL& url,
      bool show_back_button) override;
#if !BUILDFLAG(IS_ANDROID)
  sharing_hub::ScreenshotCapturedBubble* ShowScreenshotCapturedBubble(
      content::WebContents* contents,
      const gfx::Image& image) override;
  void ShowIntentPickerBubble(
      std::vector<apps::IntentPickerAppInfo> app_info,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      apps::IntentPickerBubbleType bubble_type,
      const std::optional<url::Origin>& initiating_origin,
      IntentPickerResponse callback) override {}
#endif  //  !define(OS_ANDROID)
  send_tab_to_self::SendTabToSelfBubbleView*
  ShowSendTabToSelfDevicePickerBubble(content::WebContents* contents) override;
  send_tab_to_self::SendTabToSelfBubbleView* ShowSendTabToSelfPromoBubble(
      content::WebContents* contents,
      bool show_signin_button) override;
#if BUILDFLAG(IS_CHROMEOS)
  void ToggleMultitaskMenu() override;
#else
  sharing_hub::SharingHubBubbleView* ShowSharingHubBubble(
      share::ShareAttempt attempt) override;
#endif  // BUILDFLAG(IS_CHROMEOS)
  ShowTranslateBubbleResult ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors error_type,
      bool is_user_gesture) override;
  void StartPartialTranslate(const std::string& source_language,
                             const std::string& target_language,
                             const std::u16string& text_selection) override;
  void ShowOneClickSigninConfirmation(
      const std::u16string& email,
      base::OnceCallback<void(bool)> confirmed_callback) override {}
  DownloadBubbleUIController* GetDownloadBubbleUIController() override;
  void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadCloseType dialog_type,
      base::OnceCallback<void(bool)> callback) override {}
  void UserChangedTheme(BrowserThemeChangeType theme_change_type) override {}
  std::unique_ptr<FindBar> CreateFindBar() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHostFor(
      content::WebContents* web_contents) override;
  void ShowAvatarBubbleFromAvatarButton(bool is_source_keyboard) override {}
  void MaybeShowProfileSwitchIPH() override {}
  void MaybeShowSupervisedUserProfileSignInIPH() override {}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_LINUX)
  void ShowHatsDialog(
      const std::string& site_id,
      const std::optional<std::string>& hats_histogram_name,
      const std::optional<uint64_t> hats_survey_ukm_id,
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      const SurveyBitsData& product_specific_bits_data,
      const SurveyStringData& product_specific_string_data) override {}

  void ShowIncognitoClearBrowsingDataDialog() override {}
  void ShowIncognitoHistoryDisclaimerDialog() override {}
#endif

  ExclusiveAccessContext* GetExclusiveAccessContext() override;
  std::string GetWorkspace() const override;
  bool IsVisibleOnAllWorkspaces() const override;
  void ShowEmojiPanel() override {}
  void ShowCaretBrowsingDialog() override {}
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;

  void SetNativeWindow(gfx::NativeWindow window);

  void SetCloseCallback(base::OnceClosure close_callback);

  void CreateTabSearchBubble(
      tab_search::mojom::TabSearchSection section =
          tab_search::mojom::TabSearchSection::kSearch,
      tab_search::mojom::TabOrganizationFeature feature =
          tab_search::mojom::TabOrganizationFeature::kNone) override {}
  void CloseTabSearchBubble() override {}

  bool IsTabModalPopupDeprecated() const override;
  void SetIsTabModalPopupDeprecated(
      bool is_tab_modal_popup_deprecated) override;

  void set_workspace(std::string workspace) { workspace_ = workspace; }
  void set_visible_on_all_workspaces(bool visible_on_all_workspaces) {
    visible_on_all_workspaces_ = visible_on_all_workspaces;
  }
  void set_is_active(bool active) { is_active_ = active; }
  void set_is_minimized(bool minimized) { is_minimized_ = minimized; }

  bool IsClosed() const { return is_closed_; }

 protected:
  void DeleteBrowserWindow() final;

 private:
  class TestLocationBar : public LocationBar {
   public:
    TestLocationBar() : LocationBar(/*command_updater=*/nullptr) {}
    TestLocationBar(const TestLocationBar&) = delete;
    TestLocationBar& operator=(const TestLocationBar&) = delete;
    ~TestLocationBar() override = default;

    // LocationBar:
    void FocusLocation(bool select_all) override {}
    void FocusSearch() override {}
    void UpdateContentSettingsIcons() override {}
    void SaveStateToContents(content::WebContents* contents) override {}
    void Revert() override {}
    OmniboxView* GetOmniboxView() override;
    OmniboxController* GetOmniboxController() override;
    LocationBarTesting* GetLocationBarForTesting() override;
    LocationBarModel* GetLocationBarModel() override;
    content::WebContents* GetWebContents() override;
    std::optional<bubble_anchor_util::AnchorConfiguration> GetChipAnchor()
        override;
    void OnChanged() override {}
    void UpdateWithoutTabRestore() override {}
  };

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

  autofill::TestAutofillBubbleHandler autofill_bubble_handler_;
  TestLocationBar location_bar_;
  gfx::NativeWindow native_window_ = gfx::NativeWindow();

  std::string workspace_;
  bool visible_on_all_workspaces_ = false;
  bool is_minimized_ = false;
  bool is_active_ = false;
  bool is_closed_ = false;
  bool is_tab_strip_editable_ = true;
  bool is_tab_modal_popup_deprecated_ = false;

  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observer_{this};
  raw_ptr<Browser> browser_;
  base::OnceClosure close_callback_;
};

// Helper that handle the lifetime of TestBrowserWindow instances.
std::unique_ptr<Browser> CreateBrowserWithTestWindowForParams(
    Browser::CreateParams params);

#endif  // CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
