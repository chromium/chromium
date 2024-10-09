// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
#define CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/test_download_shelf.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/buildflags.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/new_badge_controller.h"
#include "ui/base/interaction/element_identifier.h"
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

namespace user_education {
class FeaturePromoController;
}  // namespace user_education

// An implementation of BrowserWindow used for testing. TestBrowserWindow only
// contains a valid LocationBar, all other getters return NULL.
// However, some of them can be preset to a specific value.
// See BrowserWithTestWindowTest for an example of using this class.
class TestBrowserWindow : public BrowserWindow {
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
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override;
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const override;
  ui::NativeTheme* GetNativeTheme() override;
  const ui::ThemeProvider* GetThemeProvider() const override;
  const ui::ColorProvider* GetColorProvider() const override;
  ui::ElementContext GetElementContext() override;
  int GetTopControlsHeight() const override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  StatusBubble* GetStatusBubble() override;
  void UpdateTitleBar() override {}
  void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) override {}
  void TemporarilyShowBookmarkBar(base::TimeDelta duration) override {}
  void UpdateDevTools() override {}
  void UpdateLoadingAnimations(bool is_visible) override {}
  void SetStarredState(bool is_starred) override {}
  void SetTranslateIconToggled(bool is_lit) override {}
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
  void OnCanResizeFromWebAPIChanged() override {}
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
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
  bool IsBookmarkBarVisible() const override;
  bool IsBookmarkBarAnimating() const override;
  bool IsTabStripEditable() const override;
  void SetIsTabStripEditable(bool is_editable);
  bool IsToolbarVisible() const override;
  bool IsLocationBarVisible() const override;
  bool IsToolbarShowing() const override;
  bool IsBorderlessModeEnabled() const override;
  void ShowChromeLabs() override {}
  views::WebView* GetContentsWebView() override;
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
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  void VerifyUserEligibilityIOSPasswordPromoBubble() override;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  send_tab_to_self::SendTabToSelfBubbleView*
  ShowSendTabToSelfDevicePickerBubble(content::WebContents* contents) override;
  send_tab_to_self::SendTabToSelfBubbleView* ShowSendTabToSelfPromoBubble(
      content::WebContents* contents,
      bool show_signin_button) override;
#if BUILDFLAG(IS_CHROMEOS)
  views::Button* GetSharingHubIconButton() override;
  void ToggleMultitaskMenu() const override;
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
  bool IsDownloadShelfVisible() const override;
  DownloadShelf* GetDownloadShelf() override;
  views::View* GetTopContainer() override;
  DownloadBubbleUIController* GetDownloadBubbleUIController() override;
  void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadCloseType dialog_type,
      base::OnceCallback<void(bool)> callback) override {}
  void UserChangedTheme(BrowserThemeChangeType theme_change_type) override {}
  std::unique_ptr<FindBar> CreateFindBar() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  void ShowAvatarBubbleFromAvatarButton(bool is_source_keyboard) override {}
  void MaybeShowProfileSwitchIPH() override {}

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
      int tab_index = -1,
      tab_search::mojom::TabOrganizationFeature feature =
          tab_search::mojom::TabOrganizationFeature::kNone) override {}
  void CloseTabSearchBubble() override {}

  bool IsFeaturePromoActive(const base::Feature& iph_feature) const override;
  user_education::FeaturePromoResult CanShowFeaturePromo(
      const base::Feature& iph_feature) const override;
  void MaybeShowFeaturePromo(
      user_education::FeaturePromoParams params) override;
  bool MaybeShowStartupFeaturePromo(
      user_education::FeaturePromoParams params) override;
  bool AbortFeaturePromo(const base::Feature& iph_feature) override;
  user_education::FeaturePromoHandle CloseFeaturePromoAndContinue(
      const base::Feature& iph_feature) override;
  bool NotifyFeaturePromoFeatureUsed(
      const base::Feature& feature,
      FeaturePromoFeatureUsedAction action) override;
  void NotifyAdditionalConditionEvent(const char* event_name) override;
  user_education::DisplayNewBadge MaybeShowNewBadgeFor(
      const base::Feature& new_badge_feature) override;
  void NotifyNewBadgeFeatureUsed(const base::Feature& feature) override;

  // Sets the controller returned by GetFeaturePromoController().
  // Deletes the existing one, if any.
  user_education::FeaturePromoController* SetFeaturePromoController(
      std::unique_ptr<user_education::FeaturePromoController>
          feature_promo_controller);

  void set_workspace(std::string workspace) { workspace_ = workspace; }
  void set_visible_on_all_workspaces(bool visible_on_all_workspaces) {
    visible_on_all_workspaces_ = visible_on_all_workspaces;
  }
  void set_is_active(bool active) { is_active_ = active; }
  void set_is_minimized(bool minimized) { is_minimized_ = minimized; }

  bool IsClosed() const { return is_closed_; }

  void set_element_context(ui::ElementContext element_context) {
    element_context_ = element_context;
  }

 protected:
  void DestroyBrowser() override {}

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
    LocationBarTesting* GetLocationBarForTesting() override;
    LocationBarModel* GetLocationBarModel() override;
    content::WebContents* GetWebContents() override;
    void OnChanged() override {}
    void OnPopupVisibilityChanged() override {}
    void UpdateWithoutTabRestore() override {}
  };

  user_education::FeaturePromoController* GetFeaturePromoControllerImpl()
      override;

  autofill::TestAutofillBubbleHandler autofill_bubble_handler_;
  TestDownloadShelf download_shelf_{nullptr};
  TestLocationBar location_bar_;
  gfx::NativeWindow native_window_ = gfx::NativeWindow();

  std::string workspace_;
  bool visible_on_all_workspaces_ = false;
  bool is_minimized_ = false;
  bool is_active_ = false;
  bool is_closed_ = false;
  bool is_tab_strip_editable_ = true;

  std::unique_ptr<user_education::FeaturePromoController>
      feature_promo_controller_;

  ui::ElementContext element_context_;
  base::OnceClosure close_callback_;
};

// Handles destroying a TestBrowserWindow when the Browser it is attached to is
// destroyed.
class TestBrowserWindowOwner : public BrowserListObserver {
 public:
  explicit TestBrowserWindowOwner(std::unique_ptr<TestBrowserWindow> window);
  TestBrowserWindowOwner(const TestBrowserWindowOwner&) = delete;
  TestBrowserWindowOwner& operator=(const TestBrowserWindowOwner&) = delete;
  ~TestBrowserWindowOwner() override;

 private:
  // Overridden from BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;
  std::unique_ptr<TestBrowserWindow> window_;
};

// Helper that handle the lifetime of TestBrowserWindow instances.
std::unique_ptr<Browser> CreateBrowserWithTestWindowForParams(
    Browser::CreateParams params);

#endif  // CHROME_TEST_BASE_TEST_BROWSER_WINDOW_H_
