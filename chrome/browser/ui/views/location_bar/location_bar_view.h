// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"
#include "chrome/browser/ui/page_action/page_action_icon_container.h"
#include "chrome/browser/ui/views/dropdown_bar_host.h"
#include "chrome/browser/ui/views/dropdown_bar_host_delegate.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/prefs/pref_member.h"
#include "components/security_state/core/security_state.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/drag_controller.h"

class CommandUpdater;
class ContentSettingBubbleModelDelegate;
class GURL;
class IntentPickerView;
class KeywordHintView;
class LocationIconView;
class ManagePasswordsIconViews;
enum class OmniboxPart;
class OmniboxPopupView;
enum class OmniboxTint;
class PageActionIconContainerView;
class Profile;
class SelectedKeywordView;
class StarView;
class TranslateIconView;

namespace autofill {
class LocalCardMigrationIconView;
class SaveCardIconView;
}

namespace views {
class ImageButton;
class Label;
}

/////////////////////////////////////////////////////////////////////////////
//
// LocationBarView class
//
//   The LocationBarView class is a View subclass that paints the background
//   of the URL bar strip and contains its content.
//
/////////////////////////////////////////////////////////////////////////////
class LocationBarView : public LocationBar,
                        public LocationBarTesting,
                        public views::View,
                        public views::DragController,
                        public gfx::AnimationDelegate,
                        public ChromeOmniboxEditController,
                        public DropdownBarHostDelegate,
                        public views::ButtonListener,
                        public ContentSettingImageView::Delegate,
                        public PageActionIconView::Delegate,
                        public ui::MaterialDesignControllerObserver {
 public:
  class Delegate {
   public:
    // Should return the current web contents.
    virtual content::WebContents* GetWebContents() = 0;

    virtual ToolbarModel* GetToolbarModel() = 0;
    virtual const ToolbarModel* GetToolbarModel() const = 0;

    // Returns ContentSettingBubbleModelDelegate.
    virtual ContentSettingBubbleModelDelegate*
        GetContentSettingBubbleModelDelegate() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // The location bar view's class name.
  static const char kViewClassName[];

  LocationBarView(Browser* browser,
                  Profile* profile,
                  CommandUpdater* command_updater,
                  Delegate* delegate,
                  bool is_popup_mode);

  ~LocationBarView() override;

  // Returns the location bar border radius in DIPs.
  int GetBorderRadius() const;

  // Initializes the LocationBarView.
  void Init();

  // True if this instance has been initialized by calling Init, which can only
  // be called when the receiving instance is attached to a view container.
  bool IsInitialized() const;

  // Helper to get the color for |part| using the current tint().
  SkColor GetColor(OmniboxPart part) const;

  // Returns the location bar border color blended with the toolbar color.
  // It's guaranteed to be opaque.
  SkColor GetOpaqueBorderColor(bool incognito) const;

  // Returns the color to be used for the security chip in the context of
  // |security_level|.
  SkColor GetSecurityChipColor(
      security_state::SecurityLevel security_level) const;

  // Returns the color to use for icon ink highlights.
  SkColor GetIconInkDropColor() const;

  // Returns the cached theme color tint for the location bar and results.
  OmniboxTint tint() const { return tint_; }

  // Returns a background that paints an (optionally stroked) rounded rect with
  // the given color.
  std::unique_ptr<views::Background> CreateRoundRectBackground(
      SkColor background_color,
      SkColor stroke_color,
      SkBlendMode blend_mode = SkBlendMode::kSrcOver,
      bool antialias = true) const;

  // Returns the delegate.
  Delegate* delegate() const { return delegate_; }

  // The passwords icon. It may not be visible.
  ManagePasswordsIconViews* manage_passwords_icon_view() {
    return manage_passwords_icon_view_;
  }

  // Toggles the star on or off.
  void SetStarToggled(bool on);

#if defined(OS_CHROMEOS)
  // The intent picker, should not always be visible.  It will be null when
  // |browser_| is null.
  IntentPickerView* intent_picker_view() { return intent_picker_view_; }
#endif  // defined(OS_CHROMEOS)

  // The star. It may not be visible.  It will be null when |browser_| is null.
  StarView* star_view() { return star_view_; }

  // The save credit card icon. It may not be visible.  It will be null when
  // |browser_| is null.
  autofill::SaveCardIconView* save_credit_card_icon_view() {
    return save_credit_card_icon_view_;
  }

  autofill::LocalCardMigrationIconView* local_card_migration_icon_view() {
    return local_card_migration_icon_view_;
  }

  // The translate icon. It may not be visible.
  TranslateIconView* translate_icon_view() { return translate_icon_view_; }

  PageActionIconContainerView* page_action_icon_container_view() {
    return page_action_icon_container_view_;
  }

  // Returns the screen coordinates of the omnibox (where the URL text appears,
  // not where the icons are shown).
  gfx::Point GetOmniboxViewOrigin() const;

  // Shows |text| as an inline autocompletion.  This is useful for IMEs, where
  // we can't show the autocompletion inside the actual OmniboxView.  See
  // comments on |ime_inline_autocomplete_view_|.
  void SetImeInlineAutocompletion(const base::string16& text);

  // Select all of the text. Needed when the user tabs through controls
  // in the toolbar in full keyboard accessibility mode.
  virtual void SelectAll();

  LocationIconView* location_icon_view() { return location_icon_view_; }

  SelectedKeywordView* selected_keyword_view() {
    return selected_keyword_view_;
  }

  // Show a page info dialog for |web_contents|.
  // Returns true if a dialog was shown, false otherwise.
  bool ShowPageInfoDialog(content::WebContents* web_contents);

  OmniboxViewViews* omnibox_view() { return omnibox_view_; }
  const OmniboxViewViews* omnibox_view() const { return omnibox_view_; }

  // Updates the controller, and, if |contents| is non-null, restores saved
  // state that the tab holds.
  void Update(const content::WebContents* contents);

  // Clears the location bar's state for |contents|.
  void ResetTabState(content::WebContents* contents);

  // Activates the first visible but inactive PageActionIconView for
  // accessibility.
  bool ActivateFirstInactiveBubbleForAccessibility();

  // LocationBar:
  void FocusLocation(bool select_all) override;
  void Revert() override;
  OmniboxView* GetOmniboxView() override;

  // views::View:
  bool HasFocus() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void OnThemeChanged() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // ChromeOmniboxEditController:
  void UpdateWithoutTabRestore() override;
  ToolbarModel* GetToolbarModel() override;
  content::WebContents* GetWebContents() override;

  // ContentSettingImageView::Delegate:
  SkColor GetContentSettingInkDropColor() const override;
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  static bool IsVirtualKeyboardVisible(views::Widget* widget);

  // Returns the height available for user-entered text in the location bar.
  static int GetAvailableTextHeight();

  // Returns the height available for text within location bar decorations.
  static int GetAvailableDecorationTextHeight();

  void OnOmniboxFocused();
  void OnOmniboxBlurred();

  // Called when omnibox view receives mouse notifications relevant to hover.
  // |is_hovering| should be true when mouse is in omnibox; false when exited.
  void OnOmniboxHovered(bool is_hovering);

  Browser* browser() { return browser_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(SecurityIndicatorTest, CheckIndicatorText);
  FRIEND_TEST_ALL_PREFIXES(TouchLocationBarViewBrowserTest,
                           OmniboxViewViewsSize);
  FRIEND_TEST_ALL_PREFIXES(TouchLocationBarViewBrowserTest,
                           IMEInlineAutocompletePosition);
  using ContentSettingViews = std::vector<ContentSettingImageView*>;

  // Helper for GetMinimumWidth().  Calculates the incremental minimum width
  // |view| should add to the trailing width after the omnibox.
  int IncrementalMinimumWidth(views::View* view) const;

  // The border color, drawn on top of the toolbar.
  SkColor GetBorderColor() const;

  // The LocationBarView bounds, without the ends which have a border radius.
  // E.g., if the LocationBarView was 50dip long, and the border radius was 2,
  // this method would return a gfx::Rect with 46dip width.
  gfx::Rect GetLocalBoundsWithoutEndcaps() const;

  // Updates the background on a theme change, or dropdown state change.
  void RefreshBackground();

  // Updates |location_icon_view_| based on the current state and theme.
  void RefreshLocationIcon();

  // Handles the arrival of an asynchronously fetched location bar icon.
  void OnLocationIconFetched(const gfx::Image& image);

  // Updates the visibility state of the Content Blocked icons to reflect what
  // is actually blocked on the current page. Returns true if the visibility
  // of at least one of the views in |content_setting_views_| changed.
  bool RefreshContentSettingViews();

  // Updates the visibility state of the PageActionIconViews to reflect what
  // actions are available on the current page.
  void RefreshPageActionIconViews();

  // Updates the color of the icon for the "clear all" button.
  void RefreshClearAllButtonIcon();

  // Updates the focus ring.
  void RefreshFocusRing();

  // Returns text to be placed in the location icon view.
  // - For secure/insecure pages, returns text describing the URL's security
  // level.
  // - For extension URLs, returns the extension name.
  // - For chrome:// URLs, returns the short product name (e.g. Chrome).
  // - For file:// URLs, returns the text "File".
  base::string16 GetLocationIconText() const;

  // Returns true if a keyword is selected in the model.
  bool ShouldShowKeywordBubble() const;

  // Returns true if any of the following is true:
  // - the current page is explicitly secure or insecure.
  // - the current page URL is a chrome-extension:// URL.
  bool ShouldShowLocationIconText() const;

  // Returns true if the location icon text should be animated.
  bool ShouldAnimateLocationIconTextVisibilityChange() const;

  // Gets the OmniboxPopupView associated with the model in |omnibox_view_|.
  OmniboxPopupView* GetOmniboxPopupView();

  // Gets the theme color tint for the location bar and results.
  OmniboxTint GetTint();

  // LocationBar:
  GURL GetDestinationURL() const override;
  WindowOpenDisposition GetWindowOpenDisposition() const override;
  ui::PageTransition GetPageTransition() const override;
  base::TimeTicks GetMatchSelectionTimestamp() const override;
  void AcceptInput() override;
  void AcceptInput(base::TimeTicks match_selection_timestamp) override;
  void FocusSearch() override;
  void UpdateContentSettingsIcons() override;
  void UpdateManagePasswordsIconAndBubble() override;
  void UpdateSaveCreditCardIcon() override;
  void UpdateLocalCardMigrationIcon() override;
  void UpdateBookmarkStarVisibility() override;
  void UpdateLocationBarVisibility(bool visible, bool animation) override;
  void SaveStateToContents(content::WebContents* contents) override;
  const OmniboxView* GetOmniboxView() const override;
  LocationBarTesting* GetLocationBarForTesting() override;

  // LocationBarTesting:
  bool GetBookmarkStarVisibility() override;
  bool TestContentSettingImagePressed(size_t index) override;
  bool IsContentSettingBubbleShowing(size_t index) override;

  // views::View:
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  void OnFocus() override;
  void OnPaintBorder(gfx::Canvas* canvas) override;

  // views::DragController:
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  // PageActionIconView::Delegate:
  SkColor GetPageActionInkDropColor() const override;
  content::WebContents* GetWebContentsForPageActionIconView() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // ChromeOmniboxEditController:
  void OnChanged() override;
  void OnPopupVisibilityChanged() override;
  const ToolbarModel* GetToolbarModel() const override;

  // DropdownBarHostDelegate:
  void SetFocusAndSelection(bool select_all) override;

  // ui::MaterialDesignControllerObserver:
  void OnTouchUiChanged() override;

  // The Browser this LocationBarView is in.  Note that at least
  // chromeos::SimpleWebViewDialog uses a LocationBarView outside any browser
  // window, so this may be NULL.
  Browser* const browser_;

  OmniboxViewViews* omnibox_view_ = nullptr;

  // Our delegate.
  Delegate* delegate_;

  // An icon to the left of the edit field: the HTTPS lock, blank page icon,
  // search icon, EV HTTPS bubble, etc.
  LocationIconView* location_icon_view_ = nullptr;

  // A view to show inline autocompletion when an IME is active.  In this case,
  // we shouldn't change the text or selection inside the OmniboxView itself,
  // since this will conflict with the IME's control over the text.  So instead
  // we show any autocompletion in a separate field after the OmniboxView.
  views::Label* ime_inline_autocomplete_view_ = nullptr;

  // The following views are used to provide hints and remind the user as to
  // what is going in the edit. They are all added a children of the
  // LocationBarView. At most one is visible at a time. Preference is
  // given to the keyword_view_, then hint_view_.
  // These autocollapse when the edit needs the room.

  // Shown if the user has selected a keyword.
  SelectedKeywordView* selected_keyword_view_ = nullptr;

  // Shown if the selected url has a corresponding keyword.
  KeywordHintView* keyword_hint_view_ = nullptr;

  // The content setting views.
  ContentSettingViews content_setting_views_;

  // The page action icons.
  PageActionIconContainerView* page_action_icon_container_view_ = nullptr;

  // The manage passwords icon.
  ManagePasswordsIconViews* manage_passwords_icon_view_ = nullptr;

  // The save credit card icon.  It will be null when |browser_| is null.
  autofill::SaveCardIconView* save_credit_card_icon_view_ = nullptr;

  // The icon for the local card migration prompt.
  autofill::LocalCardMigrationIconView* local_card_migration_icon_view_ =
      nullptr;

  // The icon for Translate.
  TranslateIconView* translate_icon_view_ = nullptr;

#if defined(OS_CHROMEOS)
  // The intent picker for accessing ARC's apps.  It will be null when
  // |browser_| is null.
  IntentPickerView* intent_picker_view_ = nullptr;
#endif  // defined(OS_CHROMEOS)

  // The star for bookmarking.  It will be null when |browser_| is null.
  StarView* star_view_ = nullptr;

  // An [x] that appears in touch mode (when the OSK is visible) and allows the
  // user to clear all text.
  views::ImageButton* clear_all_button_ = nullptr;

  // Animation to control showing / hiding the location bar.
  gfx::SlideAnimation size_animation_{this};

  // Animation to change whole location bar background color on hover.
  gfx::SlideAnimation hover_animation_{this};

  // Whether we're in popup mode. This value also controls whether the location
  // bar is read-only.
  const bool is_popup_mode_;

  // The theme tint. Updated based on the profile and theme settings.
  OmniboxTint tint_;

  // Tracks this preference to determine whether bookmark editing is allowed.
  BooleanPrefMember edit_bookmarks_enabled_;

  // A list of all page action icons that haven't yet migrated into the
  // PageActionIconContainerView (https://crbug.com/788051), ordered by focus.
  std::vector<PageActionIconView*> page_action_icons_;

  // The security level when the location bar was last updated. Used to decide
  // whether to animate security level transitions.
  security_state::SecurityLevel last_update_security_level_ =
      security_state::NONE;

  // The focus ring, if one is in use.
  std::unique_ptr<views::FocusRing> focus_ring_;

  ScopedObserver<ui::MaterialDesignController,
                 ui::MaterialDesignControllerObserver>
      md_observer_{this};

  // Used to scope the lifetime of asynchronous icon fetch callbacks to the
  // lifetime of the object. Weak pointers issued by this factory are
  // invalidated whenever we start a new icon fetch, so don't use this weak
  // factory for any other purposes.
  base::WeakPtrFactory<LocationBarView> icon_fetch_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LocationBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_LOCATION_BAR_VIEW_H_
