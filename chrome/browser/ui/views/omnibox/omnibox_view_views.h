// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/range/range.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/base/ime/ash/input_method_manager.h"
#endif

class LocationBarView;
class OmniboxClient;

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
class OmniboxViewViews
    : public OmniboxView,
      public views::Textfield,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      public ash::input_method::InputMethodManager::CandidateWindowObserver,
#endif
      public views::TextfieldController,
      public ui::CompositorObserver,
      public TemplateURLServiceObserver {
  METADATA_HEADER(OmniboxViewViews, views::Textfield)

 public:
  // Max width of the gradient mask used to smooth ElideAnimation edges.
  static const int kSmoothingGradientMaxWidth = 15;

  OmniboxViewViews(std::unique_ptr<OmniboxClient> client,
                   bool popup_window_mode,
                   LocationBarView* location_bar_view,
                   const gfx::FontList& font_list);
  OmniboxViewViews(const OmniboxViewViews&) = delete;
  OmniboxViewViews& operator=(const OmniboxViewViews&) = delete;
  ~OmniboxViewViews() override;

  // Initialize, create the underlying views, etc.
  void Init();

  // Exposes the RenderText for tests.
#if defined(UNIT_TEST)
  gfx::RenderText* GetRenderText() { return views::Textfield::GetRenderText(); }
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

  // Indicates if the cursor is at the end of the input. Requires that both
  // ends of the selection reside there.
  bool GetSelectionAtEnd() const;

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
  std::u16string GetText() const override;
  using OmniboxView::SetUserText;
  void SetUserText(const std::u16string& text, bool update_popup) override;
  void SetWindowTextAndCaretPos(const std::u16string& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetAdditionalText(const std::u16string& additional_text) override;
  void EnterKeywordModeForDefaultSearchProvider() override;
  bool IsSelectAll() const override;
  void GetSelectionBounds(std::u16string::size_type* start,
                          std::u16string::size_type* end) const override;
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
  void OnInputMethodChanged() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  std::u16string GetLabelForCommandId(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;

  // For testing only.
  OmniboxPopupView* GetPopupViewForTesting() const;

 protected:
  // OmniboxView:
  void UpdateSchemeStyle(const gfx::Range& range) override;

  // views::Textfield:
  void OnThemeChanged() override;
  bool IsDropCursorForInsertion() const override;

  // Wrappers around Textfield methods that tests can override.
  virtual void ApplyColor(SkColor color, const gfx::Range& range);
  virtual void ApplyStyle(gfx::TextStyle style,
                          bool value,
                          const gfx::Range& range);

 private:
  friend class TestingOmniboxView;
  FRIEND_TEST_ALL_PREFIXES(OmniboxPopupViewViewsTest, EmitAccessibilityEvents);
  // TODO(tommycli): Remove the rest of these friends after porting these
  // browser tests to unit tests.
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, CloseOmniboxPopupOnTextDrag);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, FriendlyAccessibleLabel);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, DoNotNavigateOnDrop);
  FRIEND_TEST_ALL_PREFIXES(OmniboxViewViewsTest, AyncDropCallback);

  enum class UnelisionGesture {
    HOME_KEY_PRESSED,
    MOUSE_RELEASE,
    OTHER,
  };

  // Update the field with |text| and set the selection. |ranges| should not be
  // empty; even text with no selections must have at least 1 empty range in
  // |ranges| to indicate the cursor position.
  void SetTextAndSelectedRanges(const std::u16string& text,
                                const std::vector<gfx::Range>& ranges);

  void SetSelectedRanges(const std::vector<gfx::Range>& ranges);

  // Returns the selected text.
  std::u16string GetSelectedText() const;

  // Paste text from the clipboard into the omnibox.
  // Textfields implementation of Paste() pastes the contents of the clipboard
  // as is. We want to strip whitespace and other things (see GetClipboardText()
  // for details). The function invokes OnBefore/AfterPossibleChange() as
  // necessary.
  void OnOmniboxPaste();

  // Handle keyword hint tab-to-search and tabbing through dropdown results.
  bool HandleEarlyTabActions(const ui::KeyEvent& event);

  void ClearAccessibilityLabel();

  void SetAccessibilityLabel(const std::u16string& display_text,
                             const AutocompleteMatch& match,
                             bool notify_text_changed) override;

  // Returns true if the user text was updated with the full URL (without
  // steady-state elisions).  |gesture| is the user gesture causing unelision.
  bool UnapplySteadyStateElisions(UnelisionGesture gesture);

#if BUILDFLAG(IS_MAC)
  void AnnounceFriendlySuggestionText();
#endif

  // Get the preferred text input type, this checks the IME locale on Windows.
  ui::TextInputType GetPreferredTextInputType() const;

  // OmniboxView:
  void SetCaretPos(size_t caret_pos) override;
  void UpdatePopup() override;
  void ApplyCaretVisibility() override;
  void OnTemporaryTextMaybeChanged(const std::u16string& display_text,
                                   const AutocompleteMatch& match,
                                   bool save_original_selection,
                                   bool notify_text_changed) override;
  void OnInlineAutocompleteTextMaybeChanged(
      const std::u16string& display_text,
      std::vector<gfx::Range> selections,
      const std::u16string& prefix_autocompletion,
      const std::u16string& inline_autocompletion) override;
  void OnInlineAutocompleteTextCleared() override;
  void OnRevertTemporaryText(const std::u16string& display_text,
                             const AutocompleteMatch& match) override;
  void OnBeforePossibleChange() override;
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override;
  void OnKeywordPlaceholderTextChange() override;
  gfx::NativeView GetNativeView() const override;
  void ShowVirtualKeyboardIfEnabled() override;
  void HideImeIfNeeded() override;
  int GetOmniboxTextLength() const override;
  void SetEmphasis(bool emphasize, const gfx::Range& range) override;

  // views::View
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::Textfield:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void OnFocus() override;
  void OnBlur() override;
  std::u16string GetSelectionClipboardText() const override;
  void DoInsertChar(char16_t ch) override;
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void ExecuteTextEditCommand(ui::TextEditCommand command) override;
  bool ShouldShowPlaceholderText() const override;

  void UpdateAccessibleValue() override;

  // ash::input_method::InputMethodManager::CandidateWindowObserver:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void CandidateWindowOpened(
      ash::input_method::InputMethodManager* manager) override;
  void CandidateWindowClosed(
      ash::input_method::InputMethodManager* manager) override;
#endif

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
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
  ui::mojom::DragOperation OnDrop(const ui::DropTargetEvent& event) override;
  views::View::DropCallback CreateDropCallback(
      const ui::DropTargetEvent& event) override;
  void UpdateContextMenu(ui::SimpleMenuModel* menu_contents) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int id) const override;

  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override;
  void OnCompositingStarted(ui::Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnDidPresentCompositorFrame(
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;

  // Permits launch of the external protocol handler after user actions in
  // the omnibox. The handler needs to be informed that omnibox input should
  // always be considered "user gesture-triggered", lest it always return BLOCK.
  void PermitExternalProtocolHandler();

  // Drops dragged text and updates `output_drag_op` accordingly.
  void PerformDrop(const ui::DropTargetEvent& event,
                   ui::mojom::DragOperation& output_drag_op,
                   std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner);

  // Helper method to construct part of the context menu.
  void MaybeAddSendTabToSelfItem(ui::SimpleMenuModel* menu_contents);

  // Called when the popup view becomes visible.
  void OnPopupOpened();

  // Helper for updating placeholder color depending on whether its a keyword or
  // DSE placeholder.
  void UpdatePlaceholderTextColor();

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (smaller font size). This is used for popups.
  bool popup_window_mode_;

  // Owns either an OmniboxPopupViewViews or an OmniboxPopupViewWebUI.
  std::unique_ptr<OmniboxPopupView> popup_view_;
  base::CallbackListSubscription popup_view_opened_subscription_;

  // Selection persisted across temporary text changes, like popup suggestions.
  std::vector<gfx::Range> saved_temporary_selection_ = {{}};

  // Holds the user's selection across focus changes.  There is only a saved
  // selection if this range IsValid().
  std::vector<gfx::Range> saved_selection_for_focus_change_;

  // Tracking state before and after a possible change.
  State state_before_change_;
  bool ime_composing_before_change_ = false;

  // |location_bar_view_| can be NULL in tests.
  raw_ptr<LocationBarView> location_bar_view_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  // Whether the user should be notified if the clipboard is restricted.
  bool show_rejection_ui_if_any_ = false;

  // Keep track of the word that would be selected if URL is unelided between
  // a single and double click. This is an edge case where the elided URL is
  // selected. On the double click, unelision is performed in between the first
  // and second clicks. This results in both the wrong word to be selected and
  // the wrong selection length. For example, if example.com is shown and you
  // try to double click on the "x", it unelides to https://example.com after
  // the first click, resulting in "https" being selected.
  size_t next_double_click_selection_len_ = 0;
  size_t next_double_click_selection_offset_ = 0;

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
  std::u16string friendly_suggestion_text_;

  // The number of added labelling characters before editable text begins.
  // For example,  "Google https://google.com location from history",
  // this is set to 7 (the length of "Google ").
  int friendly_suggestion_text_prefix_length_;

  base::ScopedObservation<ui::Compositor, ui::CompositorObserver>
      scoped_compositor_observation_{this};
  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      scoped_template_url_service_observation_{this};

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<OmniboxViewViews> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_
