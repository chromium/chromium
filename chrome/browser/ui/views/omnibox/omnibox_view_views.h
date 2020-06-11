// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_sub_menu_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

#if defined(OS_CHROMEOS)
#include "ui/base/ime/chromeos/input_method_manager.h"
#endif

class LocationBarView;
class OmniboxClient;
class OmniboxPopupContentsView;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class RenderText;
}  // namespace gfx

namespace ui {
class OSExchangeData;
}  // namespace ui

// Views-implementation of OmniboxView.
class OmniboxViewViews : public OmniboxView,
                         public views::Textfield,
#if defined(OS_CHROMEOS)
                         public chromeos::input_method::InputMethodManager::
                             CandidateWindowObserver,
#endif
                         public views::TextfieldController,
                         public ui::CompositorObserver,
                         public TemplateURLServiceObserver,
                         public content::WebContentsObserver {
 public:
  // The internal view class name.
  static const char kViewClassName[];

  // Range of command IDs to use for the items in the send tab to self submenu.
  static const int kMinSendTabToSelfSubMenuCommandId =
      send_tab_to_self::SendTabToSelfSubMenuModel::kMinCommandId;
  static const int kMaxSendTabToSelfSubMenuCommandId =
      send_tab_to_self::SendTabToSelfSubMenuModel::kMaxCommandId;

  OmniboxViewViews(OmniboxEditController* controller,
                   std::unique_ptr<OmniboxClient> client,
                   bool popup_window_mode,
                   LocationBarView* location_bar,
                   const gfx::FontList& font_list);
  ~OmniboxViewViews() override;

  // Initialize, create the underlying views, etc.
  void Init();

  // Exposes the RenderText for tests.
#if defined(UNIT_TEST)
  gfx::RenderText* GetRenderText() {
    return views::Textfield::GetRenderText();
  }
#endif

  // For use when switching tabs, this saves the current state onto the tab so
  // that it can be restored during a later call to Update().
  void SaveStateToTab(content::WebContents* tab);

  // Called when the window's active tab changes.
  void OnTabChanged(content::WebContents* web_contents);

  // Called to clear the saved state for |web_contents|.
  void ResetTabState(content::WebContents* web_contents);

  // Installs the placeholder text with the name of the current default search
  // provider. For example, if Google is the default search provider, this shows
  // "Search Google or type a URL" when the Omnibox is empty and unfocused.
  void InstallPlaceholderText();

  // Indicates if the cursor is at one end of the input. Requires that both
  // ends of the selection reside there.
  bool SelectionAtBeginning() const;
  bool SelectionAtEnd() const;

  // Returns the width in pixels needed to display the current text. The
  // returned value includes margins.
  int GetTextWidth() const;
  // Returns the width in pixels needed to display the current text unelided.
  int GetUnelidedTextWidth() const;

  // Returns the omnibox's width in pixels.
  int GetWidth() const;

  // OmniboxView:
  void EmphasizeURLComponents() override;
  void Update() override;
  base::string16 GetText() const override;
  using OmniboxView::SetUserText;
  void SetUserText(const base::string16& text,
                   bool update_popup) override;
  void SetWindowTextAndCaretPos(
      const base::string16& text,
      size_t caret_pos,
      bool update_popup,
      bool notify_text_changed,
      const base::string16& additional_text = base::string16()) override;
  void EnterKeywordModeForDefaultSearchProvider() override;
  bool IsSelectAll() const override;
  void GetSelectionBounds(base::string16::size_type* start,
                          base::string16::size_type* end) const override;
  size_t GetAllSelectionsLength() const override;
  void SelectAll(bool reversed) override;
  void RevertAll() override;
  void SetFocus(bool is_user_initiated) override;
  bool IsImeComposing() const override;
  gfx::NativeView GetRelativeWindowForPopup() const override;
  bool IsImeShowingPopup() const override;

  // views::Textfield:
  gfx::Size GetMinimumSize() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  ui::TextInputType GetTextInputType() const override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  bool ShouldDoLearning() override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* navigation) override;
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;

  // For testing only.
  OmniboxPopupContentsView* GetPopupContentsViewForTesting() const {
    return popup_view_.get();
  }

 protected:
  // views::Textfield:
  void OnThemeChanged() override;
  bool IsDropCursorForInsertion() const override;

 private:
  // TODO(tommycli): Remove the rest of these friends after porting these
  // browser tests to unit tests.
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, CloseOmniboxPopupOnTextDrag);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, FriendlyAccessibleLabel);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, DoNotNavigateOnDrop);

  class PathFadeAnimation;

  enum class UnelisionGesture {
    HOME_KEY_PRESSED,
    MOUSE_RELEASE,
    OTHER,
  };

  // Update the field with |text| and set the selection. |ranges| should not be
  // empty; even text with no selections must have at least 1 empty range in
  // |ranges| to indicate the cursor position.
  void SetTextAndSelectedRanges(
      const base::string16& text,
      const std::vector<gfx::Range>& ranges,
      const base::string16& additional_text = base::string16());

  void SetSelectedRanges(const std::vector<gfx::Range>& ranges);

  // Returns the selected text.
  base::string16 GetSelectedText() const;

  // Paste text from the clipboard into the omnibox.
  // Textfields implementation of Paste() pastes the contents of the clipboard
  // as is. We want to strip whitespace and other things (see GetClipboardText()
  // for details). The function invokes OnBefore/AfterPossibleChange() as
  // necessary.
  void OnOmniboxPaste();

  // Handle keyword hint tab-to-search and tabbing through dropdown results.
  bool HandleEarlyTabActions(const ui::KeyEvent& event);

  void ClearAccessibilityLabel();

  void SetAccessibilityLabel(const base::string16& display_text,
                             const AutocompleteMatch& match) override;

  // Returns true if the user text was updated with the full URL (without
  // steady-state elisions).  |gesture| is the user gesture causing unelision.
  bool UnapplySteadyStateElisions(UnelisionGesture gesture);

  // Informs if text and UI direction match (otherwise what "at end" means must
  // flip.)
  bool TextAndUIDirectionMatch() const;

  // Like SelectionAtEnd(), but accounts for RTL.
  bool DirectionAwareSelectionAtEnd() const;

  // If the Secondary button for the current suggestion is focused, clicks it
  // and returns true.
  bool MaybeTriggerSecondaryButton(const ui::KeyEvent& event);

#if defined(OS_MACOSX)
  void AnnounceFriendlySuggestionText();
#endif

  // OmniboxView:
  void SetCaretPos(size_t caret_pos) override;
  void UpdatePopup() override;
  void ApplyCaretVisibility() override;
  void OnTemporaryTextMaybeChanged(const base::string16& display_text,
                                   const AutocompleteMatch& match,
                                   bool save_original_selection,
                                   bool notify_text_changed) override;
  void OnInlineAutocompleteTextMaybeChanged(
      const base::string16& display_text,
      size_t user_text_length,
      size_t user_text_start = 0,
      const base::string16& additional_text = base::string16()) override;
  void OnInlineAutocompleteTextCleared() override;
  void OnRevertTemporaryText(const base::string16& display_text,
                             const AutocompleteMatch& match) override;
  void OnBeforePossibleChange() override;
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override;
  gfx::NativeView GetNativeView() const override;
  void ShowVirtualKeyboardIfEnabled() override;
  void HideImeIfNeeded() override;
  int GetOmniboxTextLength() const override;
  void SetEmphasis(bool emphasize, const gfx::Range& range) override;
  void UpdateSchemeStyle(const gfx::Range& range) override;

  // views::View
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::Textfield:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  const char* GetClassName() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void OnFocus() override;
  void OnBlur() override;
  base::string16 GetSelectionClipboardText() const override;
  void DoInsertChar(base::char16 ch) override;
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void ExecuteTextEditCommand(ui::TextEditCommand command) override;
  bool ShouldShowPlaceholderText() const override;

  // chromeos::input_method::InputMethodManager::CandidateWindowObserver:
#if defined(OS_CHROMEOS)
  void CandidateWindowOpened(
      chromeos::input_method::InputMethodManager* manager) override;
  void CandidateWindowClosed(
      chromeos::input_method::InputMethodManager* manager) override;
#endif

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void OnBeforeUserAction(views::Textfield* sender) override;
  void OnAfterUserAction(views::Textfield* sender) override;
  void OnAfterCutOrCopy(ui::ClipboardBuffer clipboard_buffer) override;
  void OnWriteDragData(ui::OSExchangeData* data) override;
  void OnGetDragOperationsForTextfield(int* drag_operations) override;
  void AppendDropFormats(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types) override;
  int OnDrop(const ui::OSExchangeData& data) override;
  void UpdateContextMenu(ui::SimpleMenuModel* menu_contents) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int id) const override;

  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override;
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnCompositingEnded(ui::Compositor* compositor) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

  // Returns the bounds from the end of the currently displayed URL's host to
  // the end of the URL.
  gfx::Range GetPathBounds();

  // Returns true if the currently displayed URL's path is eligible for fading.
  // This takes into account the omnibox's current state (e.g. the path
  // shouldn't fade if the user is currently editing it) as well as properties
  // of the current text (e.g. extension URLs or non-URLs shouldn't have their
  // paths faded).
  bool CanFadePath();

  // When certain field trials are enabled, the URL's path is shown on page load
  // and faded out when the user interacts with the page. This method resets
  // back to the on-page-load state. That is, it unhides the path (if currently
  // hidden) and resets state so that the path will show until user interaction.
  void ResetToHideOnInteraction();

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (smaller font size). This is used for popups.
  bool popup_window_mode_;

  std::unique_ptr<OmniboxPopupContentsView> popup_view_;

  // Animations used to fade in/out the path under some elision settings.

  // Fades the path in after a short delay. Under certain variations, this
  // animation is not created until the user interacts with the page, so it's
  // not always guaranteed to exist.
  std::unique_ptr<PathFadeAnimation> path_fade_in_animation_;
  // Waits a few seconds and then fades the path out.
  std::unique_ptr<PathFadeAnimation> path_fade_out_animation_;
  // Fades the path out without a delay.
  std::unique_ptr<PathFadeAnimation> path_fade_out_fast_animation_;

  // Selection persisted across temporary text changes, like popup suggestions.
  std::vector<gfx::Range> saved_temporary_selection_;

  // Holds the user's selection across focus changes.  There is only a saved
  // selection if this range IsValid().
  std::vector<gfx::Range> saved_selection_for_focus_change_;

  // Tracking state before and after a possible change.
  State state_before_change_;
  bool ime_composing_before_change_ = false;

  // |location_bar_view_| can be NULL in tests.
  LocationBarView* location_bar_view_;

#if defined(OS_CHROMEOS)
  // True if the IME candidate window is open. When this is true, we want to
  // avoid showing the popup. So far, the candidate window is detected only
  // on Chrome OS.
  bool ime_candidate_window_open_ = false;
#endif

  // True if any mouse button is currently depressed.
  bool is_mouse_pressed_ = false;

  // Applies a minimum threshold to drag events after unelision. Because the
  // text shifts after unelision, we don't want unintentional mouse drags to
  // change the selection.
  bool filter_drag_events_for_unelision_ = false;

  // Should we select all the text when we see the mouse button get released?
  // We select in response to a click that focuses the omnibox, but we defer
  // until release, setting this variable back to false if we saw a drag, to
  // allow the user to select just a portion of the text.
  bool select_all_on_mouse_release_ = false;

  // Indicates if we want to select all text in the omnibox when we get a
  // GESTURE_TAP. We want to select all only when the textfield is not in focus
  // and gets a tap. So we use this variable to remember focus state before tap.
  bool select_all_on_gesture_tap_ = false;

  // The time of the first character insert operation that has not yet been
  // painted. Used to measure omnibox responsiveness with a histogram.
  base::TimeTicks insert_char_time_;

  // The state machine for logging the Omnibox.CharTypedToRepaintLatency
  // histogram.
  enum {
    NOT_ACTIVE,           // Not currently tracking a char typed event.
    CHAR_TYPED,           // Character was typed.
    ON_PAINT_CALLED,      // Character was typed and OnPaint() called.
    COMPOSITING_COMMIT,   // Compositing was committed after OnPaint().
    COMPOSITING_STARTED,  // Compositing was started.
  } latency_histogram_state_;

  // The currently selected match, if any, with additional labelling text
  // such as the document title and the type of search, for example:
  // "Google https://google.com location from bookmark", or
  // "cats are liquid search suggestion".
  base::string16 friendly_suggestion_text_;

  // The number of added labelling characters before editable text begins.
  // For example,  "Google https://google.com location from history",
  // this is set to 7 (the length of "Google ").
  int friendly_suggestion_text_prefix_length_;

  ScopedObserver<ui::Compositor, ui::CompositorObserver>
      scoped_compositor_observer_{this};
  ScopedObserver<TemplateURLService, TemplateURLServiceObserver>
      scoped_template_url_service_observer_{this};

  // Send tab to self submenu.
  std::unique_ptr<send_tab_to_self::SendTabToSelfSubMenuModel>
      send_tab_to_self_sub_menu_model_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<OmniboxViewViews> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_
