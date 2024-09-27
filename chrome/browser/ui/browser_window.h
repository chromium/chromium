// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#include "chrome/browser/lifetime/browser_close_manager.h"
#include "chrome/browser/share/share_attempt.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/common/buildflags.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/translate/core/common/translate_errors.h"
#include "ui/base/base_window.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class Browser;
class BrowserView;
class DownloadBubbleUIController;
class DownloadShelf;
class ExclusiveAccessContext;
class ExtensionsContainer;
class FindBar;
class GURL;
class LocationBar;
class SharingDialog;
class StatusBubble;
struct SharingDialogData;

namespace autofill {
class AutofillBubbleHandler;
}  // namespace autofill

namespace content {
class WebContents;
struct NativeWebKeyboardEvent;
enum class KeyboardEventProcessingResult;
}  // namespace content

namespace gfx {
class Size;
}

namespace qrcode_generator {
class QRCodeGeneratorBubbleView;
}  // namespace qrcode_generator

namespace send_tab_to_self {
class SendTabToSelfBubbleView;
}  // namespace send_tab_to_self

namespace sharing_hub {
class ScreenshotCapturedBubble;
class SharingHubBubbleView;
}  // namespace sharing_hub

namespace signin_metrics {
enum class AccessPoint;
}

namespace ui {
class ColorProvider;
class NativeTheme;
class ThemeProvider;
}

namespace views {
class Button;
class WebView;
}  // namespace views

namespace web_modal {
class WebContentsModalDialogHost;
}

enum class ShowTranslateBubbleResult {
  // The Full Page Translate bubble was successfully shown.
  SUCCESS,

  // The various reasons for which the Full Page Translate bubble could fail to
  // be shown.
  BROWSER_WINDOW_NOT_VALID,
  BROWSER_WINDOW_MINIMIZED,
  BROWSER_WINDOW_NOT_ACTIVE,
  WEB_CONTENTS_NOT_ACTIVE,
  EDITABLE_FIELD_IS_ACTIVE,
};

enum class BrowserThemeChangeType {
  // User changes the browser theme.
  kBrowserTheme,
  // User changes the OS native theme.
  kNativeTheme,
  // A web app sets a theme color at launch, or changes theme color.
  kWebAppTheme
};

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow interface
//  An interface implemented by the "view" of the Browser window.
//  This interface includes ui::BaseWindow methods as well as Browser window
//  specific methods.
//
// NOTE: All getters may return NULL.
//
class BrowserWindow : public ui::BaseWindow,
                      public BrowserUserEducationInterface {
 public:
  ~BrowserWindow() override = default;

  //////////////////////////////////////////////////////////////////////////////
  // ui::BaseWindow interface notes:

  // Closes the window as soon as possible. If the window is not in a drag
  // session, it will close immediately; otherwise, it will move offscreen (so
  // events are still fired) until the drag ends, then close. This assumes
  // that the Browser is not immediately destroyed, but will be eventually
  // destroyed by other means (eg, the tab strip going to zero elements).
  // Bad things happen if the Browser dtor is called directly as a result of
  // invoking this method.
  // virtual void Close() = 0;

  // Browser::OnWindowDidShow should be called after showing the window.
  // virtual void Show() = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Browser specific methods:

  // Returns the browser window currently hosting `web_contents`. If no browser
  // window exists this returns null.
  static BrowserWindow* FindBrowserWindowWithWebContents(
      content::WebContents* web_contents);

  // Returns true if the browser window is on the current workspace (a.k.a.
  // virtual desktop) or if we can't tell. False otherwise.
  //
  // On Windows, it must not be called while application is dispatching an input
  // synchronous call like SendMessage, because IsWindowOnCurrentVirtualDesktop
  // will return an error.
  virtual bool IsOnCurrentWorkspace() const = 0;

  // Sets the shown |ratio| of the browser's top controls (a.k.a. top-chrome) as
  // a result of gesture scrolling in |web_contents|.
  virtual void SetTopControlsShownRatio(content::WebContents* web_contents,
                                        float ratio) = 0;

  // Whether or not the renderer's viewport size should be shrunk by the height
  // of the browser's top controls.
  // As top-chrome is slided up or down, we don't actually resize the web
  // contents (for perf reasons) but we have to do a bunch of adjustments on the
  // renderer side to make it appear to the user like we're resizing things
  // smoothly:
  //
  // 1) Expose content beyond the web contents rect by expanding the clip.
  // 2) Push bottom-fixed elements around until we get a resize. As top-chrome
  //    hides, we push the fixed elements down by an equivalent amount so that
  //    they appear to stay fixed to the viewport bottom.
  //
  // Only when the user releases their finger to finish the scroll do we
  // actually resize the web contents and clear these adjustments. So web
  // contents has two possible sizes, viewport filling and shrunk by the top
  // controls.
  //
  // The GetTopControlsHeight is a static number that never changes (as long as
  // the top-chrome slide with gesture scrolls feature is enabled). To get the
  // actual "showing" height as the user sees, you multiply this by the shown
  // ratio. However, it's not enough to know this value, the renderer also needs
  // to know which direction it should be doing the above-mentioned adjustments.
  // That's what the DoBrowserControlsShrinkRendererSize bit is for. It tells
  // the renderer whether it's currently in the "viewport filling" or the
  // "shrunk by top controls" state.
  // The returned value should never change while sliding top-chrome is in
  // progress (either due to an in-progress gesture scroll, or due to a
  // renderer-initiated animation of the top controls shown ratio).
  virtual bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const = 0;

  // Returns the native theme associated with the frame.
  virtual ui::NativeTheme* GetNativeTheme() = 0;

  // Returns the ThemeProvider associated with the frame.
  virtual const ui::ThemeProvider* GetThemeProvider() const = 0;

  // Returns the ColorProvider associated with the frame.
  virtual const ui::ColorProvider* GetColorProvider() const = 0;

  // Returns the context for use with ElementTracker, InteractionSequence, etc.
  virtual ui::ElementContext GetElementContext() = 0;

  // Returns the height of the browser's top controls. This height doesn't
  // change with the current shown ratio above. Renderers will call this to
  // calculate the top-chrome shown ratio from the gesture scroll offset.
  //
  // Note: This should always return 0 if hiding top-chrome with page gesture
  // scrolls is disabled. This is needed so the renderer scrolls the page
  // immediately rather than changing the shown ratio, thinking that top-chrome
  // and the page's top edge are moving.
  virtual int GetTopControlsHeight() const = 0;

  // Propagates to the browser that gesture scrolling has changed state.
  virtual void SetTopControlsGestureScrollInProgress(bool in_progress) = 0;

  // Return the status bubble associated with the frame
  virtual StatusBubble* GetStatusBubble() = 0;

  // Inform the frame that the selected tab favicon or title has changed. Some
  // frames may need to refresh their title bar.
  virtual void UpdateTitleBar() = 0;

  // Invoked when the state of the bookmark bar changes. This is only invoked if
  // the state changes for the current tab, it is not sent when switching tabs.
  virtual void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) = 0;

  // Temporarily force shows the bookmark bar for the provided |duration|.
  virtual void TemporarilyShowBookmarkBar(base::TimeDelta duration) = 0;

  // Inform the frame that the dev tools window for the selected tab has
  // changed.
  virtual void UpdateDevTools() = 0;

  // Update any loading animations running in the window. |is_visible| is true
  // if the window is visible.
  virtual void UpdateLoadingAnimations(bool is_visible) = 0;

  // Sets the starred state for the current tab.
  virtual void SetStarredState(bool is_starred) = 0;

  // Sets whether the translate icon is lit for the current tab.
  virtual void SetTranslateIconToggled(bool is_lit) = 0;

  // Called when the active tab changes.  Subclasses which implement
  // TabStripModelObserver should implement this instead of ActiveTabChanged();
  // the Browser will call this method while processing that one.
  virtual void OnActiveTabChanged(content::WebContents* old_contents,
                                  content::WebContents* new_contents,
                                  int index,
                                  int reason) = 0;

  // Called when a tab is detached. Subclasses which implement
  // TabStripModelObserver should implement this instead of processing this
  // in OnTabStripModelChanged(); the Browser will call this method.
  virtual void OnTabDetached(content::WebContents* contents,
                             bool was_active) = 0;

  // Called to force the zoom state to for the active tab to be recalculated.
  // |can_show_bubble| is true when a user presses the zoom up or down keyboard
  // shortcuts and will be false in other cases (e.g. switching tabs, "clicking"
  // + or - in the app menu to change zoom).
  virtual void ZoomChangedForActiveTab(bool can_show_bubble) = 0;

  // Windows and GTK remove the browser controls in fullscreen, but Mac and Ash
  // keep the controls in a slide-down panel.
  virtual bool ShouldHideUIForFullscreen() const = 0;

  // Returns true if the fullscreen bubble is visible.
  virtual bool IsFullscreenBubbleVisible() const = 0;

  // True when we do not want to allow exiting fullscreen, e.g. in Chrome OS
  // Kiosk session.
  virtual bool IsForceFullscreen() const = 0;
  virtual void SetForceFullscreen(bool force_fullscreen) = 0;

  // Returns the size of WebContents in the browser. This may be called before
  // the TabStripModel has an active tab.
  virtual gfx::Size GetContentsSize() const = 0;

  // Resizes the window to fit a WebContents of a certain size. This should only
  // be called after the TabStripModel has an active tab.
  virtual void SetContentsSize(const gfx::Size& size) = 0;

  // Updates the visual state of the specified page action icon if present on
  // the window.
  virtual void UpdatePageActionIcon(PageActionIconType type) = 0;

  // Returns the AutofillBubbleHandler responsible for handling all
  // Autofill-related bubbles.
  virtual autofill::AutofillBubbleHandler* GetAutofillBubbleHandler() = 0;

  // Executes the action for the specified page action icon.
  virtual void ExecutePageActionIconForTesting(PageActionIconType type) = 0;

  // Returns the location bar.
  virtual LocationBar* GetLocationBar() const = 0;

  // Tries to focus the location bar.  Clears the window focus (to avoid
  // inconsistent state) if this fails.
  virtual void SetFocusToLocationBar(bool select_all) = 0;

  // Informs the view whether or not a load is in progress for the current tab.
  // The view can use this notification to update the reload/stop button.
  virtual void UpdateReloadStopState(bool is_loading, bool force) = 0;

  // Updates the toolbar with the state for the specified |contents|.
  virtual void UpdateToolbar(content::WebContents* contents) = 0;

  // Updates the toolbar's visible security state. Returns true if the toolbar
  // was redrawn.
  virtual bool UpdateToolbarSecurityState() = 0;

  // Updates whether or not the custom tab bar is visible. Animates the
  // transition if |animate| is true.
  virtual void UpdateCustomTabBarVisibility(bool visible, bool animate) = 0;

  // Resets the toolbar's tab state for |contents|.
  virtual void ResetToolbarTabState(content::WebContents* contents) = 0;

  // Focuses the toolbar (for accessibility).
  virtual void FocusToolbar() = 0;

  // Returns the ExtensionsContainer associated with the window, if any.
  virtual ExtensionsContainer* GetExtensionsContainer() = 0;

  // Called from toolbar subviews during their show/hide animations.
  virtual void ToolbarSizeChanged(bool is_animating) = 0;

  // Called when the associated window's tab dragging status changed.
  virtual void TabDraggingStatusChanged(bool is_dragging) = 0;

  // Called when a link is opened in the window from a user gesture.
  // Link will be opened with |disposition|.
  // TODO(crbug.com/40719979): see if this can't be piped through TabStripModel
  // events instead.
  virtual void LinkOpeningFromGesture(WindowOpenDisposition disposition) = 0;

  // Focuses the app menu like it was a menu bar.
  //
  // Not used on the Mac, which has a "normal" menu bar.
  virtual void FocusAppMenu() = 0;

  // Focuses the bookmarks toolbar (for accessibility).
  virtual void FocusBookmarksToolbar() = 0;

  // Focuses a visible but inactive popup for accessibility.
  virtual void FocusInactivePopupForAccessibility() = 0;

  // Moves keyboard focus to the next pane.
  virtual void RotatePaneFocus(bool forwards) = 0;

  // Moves keyboard focus directly to the web contents pane.
  virtual void FocusWebContentsPane() = 0;

  // Returns whether the bookmark bar is visible or not.
  virtual bool IsBookmarkBarVisible() const = 0;

  // Returns whether the bookmark bar is animating or not.
  virtual bool IsBookmarkBarAnimating() const = 0;

  // Returns whether the tab strip is editable (for extensions).
  virtual bool IsTabStripEditable() const = 0;

  // Returns whether the toolbar is available or not. It's called "Visible()"
  // to follow the name convention. But it does not indicate the visibility of
  // the toolbar, i.e. toolbar may be hidden, and only visible when the mouse
  // cursor is at a certain place.
  // TODO(zijiehe): Rename Visible() functions into Available() to match their
  // original meaning.
  virtual bool IsToolbarVisible() const = 0;

  // Returns whether the toolbar is showing up on the screen.
  // TODO(zijiehe): Rename this function into IsToolbarVisible() once other
  // Visible() functions are renamed to Available().
  virtual bool IsToolbarShowing() const = 0;

  // Returns whether the location bar is visible.
  virtual bool IsLocationBarVisible() const = 0;

  // Shows the dialog for a sharing feature.
  virtual SharingDialog* ShowSharingDialog(content::WebContents* contents,
                                           SharingDialogData data) = 0;

  // Shows the Update Recommended dialog box.
  virtual void ShowUpdateChromeDialog() = 0;

  // Shows the intent picker bubble. |app_info| contains the app candidates to
  // display, if |show_stay_in_chrome| is false, the 'Stay in
  // Chrome' (used for non-http(s) queries) button is hidden, if
  // |show_remember_selection| is false, the "remember my choice" checkbox is
  // hidden and |callback| helps to continue the flow back to either
  // AppsNavigationThrottle or ArcExternalProtocolDialog capturing the user's
  // decision and storing UMA metrics.
  virtual void ShowIntentPickerBubble(
      std::vector<apps::IntentPickerAppInfo> app_info,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      apps::IntentPickerBubbleType bubble_type,
      const std::optional<url::Origin>& initiating_origin,
      IntentPickerResponse callback) = 0;

  // Shows the Bookmark bubble. |url| is the URL being bookmarked,
  // |already_bookmarked| is true if the url is already bookmarked.
  virtual void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) = 0;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Checks if the user is eligible for the iOS Password Promo Bubble. If they
  // are, make a final eligibility check with a call to the segmentation
  // platform with `MaybeShowIOSPasswordPromoBubble` passed as callback. That
  // method may create/show a bubble to the user.
  virtual void VerifyUserEligibilityIOSPasswordPromoBubble() = 0;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Shows the Screenshot bubble.
  virtual sharing_hub::ScreenshotCapturedBubble* ShowScreenshotCapturedBubble(
      content::WebContents* contents,
      const gfx::Image& image) = 0;

  // Shows the QR Code generator bubble. |url| is the URL for the initial code.
  virtual qrcode_generator::QRCodeGeneratorBubbleView*
  ShowQRCodeGeneratorBubble(content::WebContents* contents,
                            const GURL& url,
                            bool show_back_button) = 0;

  // Shows the "send tab to self" device picker bubble. This must only be called
  // as a direct result of user action.
  virtual send_tab_to_self::SendTabToSelfBubbleView*
  ShowSendTabToSelfDevicePickerBubble(content::WebContents* contents) = 0;

  // Shows the "send tab to self" promo bubble. This must only be called as a
  // direct result of user action.
  virtual send_tab_to_self::SendTabToSelfBubbleView*
  ShowSendTabToSelfPromoBubble(content::WebContents* contents,
                               bool show_signin_button) = 0;

#if BUILDFLAG(IS_CHROMEOS)
  // Returns the PageActionIconView for the Sharing Hub.
  virtual views::Button* GetSharingHubIconButton() = 0;

  // Toggles the multitask menu on the browser frame size button.
  virtual void ToggleMultitaskMenu() const = 0;
#else
  // Shows the Sharing Hub bubble. This must only be called as a direct result
  // of user action.
  virtual sharing_hub::SharingHubBubbleView* ShowSharingHubBubble(
      share::ShareAttempt attempt) = 0;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Shows the Full Page Translate bubble.
  //
  // |is_user_gesture| is true when the bubble is shown on the user's deliberate
  // action.
  virtual ShowTranslateBubbleResult ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors error_type,
      bool is_user_gesture) = 0;

  // Shows the Partial Translate bubble.
  virtual void StartPartialTranslate(const std::string& source_language,
                                     const std::string& target_language,
                                     const std::u16string& text_selection) = 0;

  // Shows the one-click sign in confirmation UI. |email| holds the full email
  // address of the account that has signed in.
  virtual void ShowOneClickSigninConfirmation(
      const std::u16string& email,
      base::OnceCallback<void(bool)> confirmed_callback) = 0;

  // Whether or not the shelf view is visible.
  virtual bool IsDownloadShelfVisible() const = 0;

  // Returns the DownloadShelf. Returns null if download shelf is disabled. This
  // can happen if the new download bubble UI is enabled.
  virtual DownloadShelf* GetDownloadShelf() = 0;

  // Returns the TopContainerView.
  virtual views::View* GetTopContainer() = 0;

  // Returns the DownloadBubbleUIController. Returns null if Download Bubble
  // UI is not enabled, or if the download toolbar button does not exist.
  virtual DownloadBubbleUIController* GetDownloadBubbleUIController() = 0;

  // Shows the confirmation dialog box warning that the browser is closing with
  // in-progress downloads.
  // This method should call |callback| with the user's response.
  virtual void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadCloseType dialog_type,
      base::OnceCallback<void(bool)> callback) = 0;

  // ThemeService calls this when a user has changed their theme, indicating
  // that it's time to redraw everything.
  virtual void UserChangedTheme(BrowserThemeChangeType theme_change_type) = 0;

  // Shows the app menu (for accessibility).
  virtual void ShowAppMenu() = 0;

  // Allows the BrowserWindow object to handle the specified keyboard event
  // before sending it to the renderer.
  virtual content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) = 0;

  // Allows the BrowserWindow object to handle the specified keyboard event,
  // if the renderer did not process it.
  virtual bool HandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) = 0;

  // Construct a FindBar implementation for the |browser|.
  virtual std::unique_ptr<FindBar> CreateFindBar() = 0;

  // Return the WebContentsModalDialogHost for use in positioning web contents
  // modal dialogs within the browser window. This can sometimes be NULL (for
  // instance during tab drag on Views/Win32).
  virtual web_modal::WebContentsModalDialogHost*
  GetWebContentsModalDialogHost() = 0;

  // Construct a BrowserWindow implementation for the specified |browser|.
  static BrowserWindow* CreateBrowserWindow(std::unique_ptr<Browser> browser,
                                            bool user_gesture,
                                            bool in_tab_dragging);

  virtual void ShowAvatarBubbleFromAvatarButton(bool is_source_accelerator) = 0;

  // Attempts showing the In-Produce-Help for profile Switching. This is called
  // after creating a new profile or opening an existing profile. If the profile
  // customization bubble is shown, the IPH should be shown after.
  virtual void MaybeShowProfileSwitchIPH() = 0;

  // Shows User Happiness Tracking Survey's dialog after the survey associated
  // with |site_id| has been successfully loaded. Failure to load the survey
  // will result in the dialog not being shown. |product_specific_bits_data| and
  // |product_specific_string_data| should contain key-value pairs where the
  // keys match the field names set for the survey in hats_service.cc, and the
  // values are those which will be associated with the survey response.
  // The parameters |hats_histogram_name| and |hats_survey_ukm_id| allows HaTS
  // to log to UMA and UKM, respectively. These values are are populated in
  // chrome/browser/ui/hats/survey_config.cc and can be configured in finch.
  // Surveys that opt-in to UMA and UKM will need to have surveys reviewed by
  // privacy to ensure they are appropriate to log to UMA and/or UKM. This is
  // enforced through the OWNERS mechanism.
  virtual void ShowHatsDialog(
      const std::string& site_id,
      const std::optional<std::string>& hats_histogram_name,
      const std::optional<uint64_t> hats_survey_ukm_id,
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      const SurveyBitsData& product_specific_bits_data,
      const SurveyStringData& product_specific_string_data) = 0;

  // Returns object implementing ExclusiveAccessContext interface.
  virtual ExclusiveAccessContext* GetExclusiveAccessContext() = 0;

  // Returns the platform-specific ID of the workspace the browser window
  // currently resides in.
  virtual std::string GetWorkspace() const = 0;
  virtual bool IsVisibleOnAllWorkspaces() const = 0;

  // Shows the platform specific emoji picker.
  virtual void ShowEmojiPanel() = 0;

  // Opens the eye dropper.
  virtual std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) = 0;

  // Shows a confirmation dialog about enabling caret browsing.
  virtual void ShowCaretBrowsingDialog() = 0;

  // Create and open the tab search bubble. Optionally force it to open to the
  // given tab index and organization feature index.
  virtual void CreateTabSearchBubble(
      int tab_index,
      tab_search::mojom::TabOrganizationFeature organization_feature) = 0;
  // Closes the tab search bubble if open for the given browser instance.
  virtual void CloseTabSearchBubble() = 0;

  // Shows an Incognito clear browsing data dialog.
  virtual void ShowIncognitoClearBrowsingDataDialog() = 0;

  // Shows an Incognito history disclaimer dialog.
  virtual void ShowIncognitoHistoryDisclaimerDialog() = 0;

  // Returns true when the borderless mode should be displayed instead
  // of a full titlebar. This is only supported for desktop web apps.
  virtual bool IsBorderlessModeEnabled() const = 0;

  // Notifies `BrowserView` about the resizable boolean having been set vith
  // `window.setResizable(bool)` API.
  virtual void OnCanResizeFromWebAPIChanged() = 0;

  // Returns the overall resizability of the `BrowserView` when considering
  // both the value set by the `window.setResizable(bool)` API and browser's
  // "native" resizability.
  virtual bool GetCanResize() = 0;

  virtual ui::mojom::WindowShowState GetWindowShowState() const = 0;

  // Shows the Chrome Labs bubble if enabled.
  virtual void ShowChromeLabs() = 0;

  // Returns the WebView backing the tab-contents area of the BrowserWindow.
  virtual views::WebView* GetContentsWebView() = 0;

  // In production code BrowserView is the only subclass for BrowserWindow. The
  // fact that this is not true in some tests is a problem with the tests. See
  // https://crbug.com/360163254.
  virtual BrowserView* AsBrowserView() = 0;

 protected:
  friend class BrowserCloseManager;
  friend class BrowserView;
  virtual void DestroyBrowser() = 0;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_H_
