// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/buildflags.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/toolbar_field_trial.h"
#include "components/omnibox/browser/toolbar_model.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_keyboard_controller.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/selection_model.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/browser/browser_process.h"
#endif

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker_factory.h"
#endif

namespace {

// OmniboxState ---------------------------------------------------------------

// Stores omnibox state for each tab.
struct OmniboxState : public base::SupportsUserData::Data {
  static const char kKey[];

  OmniboxState(const OmniboxEditModel::State& model_state,
               const gfx::Range& selection,
               const gfx::Range& saved_selection_for_focus_change);
  ~OmniboxState() override;

  const OmniboxEditModel::State model_state;

  // We store both the actual selection and any saved selection (for when the
  // omnibox is not focused).  This allows us to properly handle cases like
  // selecting text, tabbing out of the omnibox, switching tabs away and back,
  // and tabbing back into the omnibox.
  const gfx::Range selection;
  const gfx::Range saved_selection_for_focus_change;
};

// static
const char OmniboxState::kKey[] = "OmniboxState";

OmniboxState::OmniboxState(const OmniboxEditModel::State& model_state,
                           const gfx::Range& selection,
                           const gfx::Range& saved_selection_for_focus_change)
    : model_state(model_state),
      selection(selection),
      saved_selection_for_focus_change(saved_selection_for_focus_change) {
}

OmniboxState::~OmniboxState() {
}

}  // namespace


// OmniboxViewViews -----------------------------------------------------------

// static
const char OmniboxViewViews::kViewClassName[] = "OmniboxViewViews";

OmniboxViewViews::OmniboxViewViews(OmniboxEditController* controller,
                                   std::unique_ptr<OmniboxClient> client,
                                   bool popup_window_mode,
                                   LocationBarView* location_bar,
                                   const gfx::FontList& font_list)
    : OmniboxView(controller, std::move(client)),
      popup_window_mode_(popup_window_mode),
      security_level_(security_state::NONE),
      saved_selection_for_focus_change_(gfx::Range::InvalidRange()),
      ime_composing_before_change_(false),
      location_bar_view_(location_bar),
      ime_candidate_window_open_(false),
      is_mouse_pressed_(false),
      select_all_on_mouse_release_(false),
      select_all_on_gesture_tap_(false),
      latency_histogram_state_(NOT_ACTIVE),
      friendly_suggestion_text_prefix_length_(0),
      scoped_compositor_observer_(this),
      scoped_template_url_service_observer_(this) {
  set_id(VIEW_ID_OMNIBOX);
  SetFontList(font_list);
}

OmniboxViewViews::~OmniboxViewViews() {
#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager::Get()->
      RemoveCandidateWindowObserver(this);
#endif

  // Explicitly teardown members which have a reference to us.  Just to be safe
  // we want them to be destroyed before destroying any other internal state.
  popup_view_.reset();
}

void OmniboxViewViews::Init() {
  set_controller(this);
  SetTextInputType(ui::TEXT_INPUT_TYPE_URL);
  GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
  GetRenderText()->set_symmetric_selection_visual_bounds(true);

  if (popup_window_mode_)
    SetReadOnly(true);

  if (location_bar_view_) {
    InstallPlaceholderText();
    scoped_template_url_service_observer_.Add(
        model()->client()->GetTemplateURLService());

    // Initialize the popup view using the same font.
    popup_view_.reset(
        new OmniboxPopupContentsView(this, model(), location_bar_view_));
  }

  // Override the default FocusableBorder from Textfield, since the
  // LocationBarView will indicate the focus state.
  constexpr gfx::Insets kTextfieldInsets(3);
  SetBorder(views::CreateEmptyBorder(kTextfieldInsets));

#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager::Get()->
      AddCandidateWindowObserver(this);
#endif
}

void OmniboxViewViews::SaveStateToTab(content::WebContents* tab) {
  DCHECK(tab);

  // We don't want to keep the IME status, so force quit the current
  // session here.  It may affect the selection status, so order is
  // also important.
  if (IsIMEComposing()) {
    ConfirmCompositionText();
    GetInputMethod()->CancelComposition(this);
  }

  // NOTE: GetStateForTabSwitch() may affect GetSelectedRange(), so order is
  // important.
  OmniboxEditModel::State state = model()->GetStateForTabSwitch();
  tab->SetUserData(OmniboxState::kKey, std::make_unique<OmniboxState>(
                                           state, GetSelectedRange(),
                                           saved_selection_for_focus_change_));
}

void OmniboxViewViews::OnTabChanged(const content::WebContents* web_contents) {
  UpdateSecurityLevel();
  const OmniboxState* state = static_cast<OmniboxState*>(
      web_contents->GetUserData(&OmniboxState::kKey));
  model()->RestoreState(state ? &state->model_state : nullptr);
  if (state) {
    // This assumes that the omnibox has already been focused or blurred as
    // appropriate; otherwise, a subsequent OnFocus() or OnBlur() call could
    // goof up the selection.  See comments on OnActiveTabChanged() call in
    // Browser::ActiveTabChanged().
    SelectRange(state->selection);
    saved_selection_for_focus_change_ = state->saved_selection_for_focus_change;
  }

  // TODO(msw|oshima): Consider saving/restoring edit history.
  ClearEditHistory();
}

void OmniboxViewViews::ResetTabState(content::WebContents* web_contents) {
  web_contents->SetUserData(OmniboxState::kKey, nullptr);
}

void OmniboxViewViews::InstallPlaceholderText() {
  set_placeholder_text_color(
      location_bar_view_->GetColor(OmniboxPart::LOCATION_BAR_TEXT_DIMMED));

  const TemplateURL* const default_provider =
      model()->client()->GetTemplateURLService()->GetDefaultSearchProvider();
  if (default_provider) {
    set_placeholder_text(l10n_util::GetStringFUTF16(
        IDS_OMNIBOX_PLACEHOLDER_TEXT, default_provider->short_name()));
  } else {
    set_placeholder_text(base::string16());
  }
}

bool OmniboxViewViews::SelectionAtEnd() {
  const gfx::Range sel = GetSelectedRange();
  return sel.GetMax() == text().size();
}

void OmniboxViewViews::EmphasizeURLComponents() {
  if (!location_bar_view_)
    return;

  // If the current contents is a URL, turn on special URL rendering mode in
  // RenderText.
  bool text_is_url = model()->CurrentTextIsURL();
  GetRenderText()->SetDirectionalityMode(
      text_is_url ? gfx::DIRECTIONALITY_AS_URL : gfx::DIRECTIONALITY_FROM_TEXT);
  SetStyle(gfx::STRIKE, false);
  UpdateTextStyle(text(), text_is_url,
                  model()->client()->GetSchemeClassifier());
}

void OmniboxViewViews::Update() {
  const security_state::SecurityLevel old_security_level = security_level_;
  UpdateSecurityLevel();

  if (model()->ResetDisplayTexts()) {
    RevertAll();

    // Only select all when we have focus.  If we don't have focus, selecting
    // all is unnecessary since the selection will change on regaining focus,
    // and can in fact cause artifacts, e.g. if the user is on the NTP and
    // clicks a link to navigate, causing |was_select_all| to be vacuously true
    // for the empty omnibox, and we then select all here, leading to the
    // trailing portion of a long URL being scrolled into view.  We could try
    // and address cases like this, but it seems better to just not muck with
    // things when the omnibox isn't focused to begin with.
    if (model()->has_focus())
      SelectAll(true);
  } else if (old_security_level != security_level_) {
    EmphasizeURLComponents();
  }
}

base::string16 OmniboxViewViews::GetText() const {
  // TODO(oshima): IME support
  return text();
}

void OmniboxViewViews::SetUserText(const base::string16& text,
                                   bool update_popup) {
  saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  OmniboxView::SetUserText(text, update_popup);
}

void OmniboxViewViews::EnterKeywordModeForDefaultSearchProvider() {
  // Transition the user into keyword mode using their default search provider.
  model()->EnterKeywordModeForDefaultSearchProvider(
      KeywordModeEntryMethod::KEYBOARD_SHORTCUT);
}

void OmniboxViewViews::GetSelectionBounds(
    base::string16::size_type* start,
    base::string16::size_type* end) const {
  const gfx::Range range = GetSelectedRange();
  *start = static_cast<size_t>(range.start());
  *end = static_cast<size_t>(range.end());
}

void OmniboxViewViews::SelectAll(bool reversed) {
  views::Textfield::SelectAll(reversed);
}

void OmniboxViewViews::RevertAll() {
  saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  OmniboxView::RevertAll();
}

void OmniboxViewViews::SetFocus() {
  // Temporarily reveal the top-of-window views (if not already revealed) so
  // that the location bar view is visible and is considered focusable. When it
  // actually receives focus, ImmersiveFocusWatcher will add another lock to
  // keep it revealed. |location_bar_view_| can be nullptr in unit tests.
  std::unique_ptr<ImmersiveRevealedLock> focus_reveal_lock;
  if (location_bar_view_) {
    focus_reveal_lock.reset(
        BrowserView::GetBrowserViewForBrowser(location_bar_view_->browser())
            ->immersive_mode_controller()
            ->GetRevealedLock(ImmersiveModeController::ANIMATE_REVEAL_YES));
  }

  RequestFocus();
  // Restore caret visibility if focus is explicitly requested. This is
  // necessary because if we already have invisible focus, the RequestFocus()
  // call above will short-circuit, preventing us from reaching
  // OmniboxEditModel::OnSetFocus(), which handles restoring visibility when the
  // omnibox regains focus after losing focus.
  model()->SetCaretVisibility(true);
  // If the user attempts to focus the omnibox, and the ctrl key is pressed, we
  // want to prevent ctrl-enter behavior until the ctrl key is released and
  // re-pressed. This occurs even if the omnibox is already focused and we
  // re-request focus (e.g. pressing ctrl-l twice).
  model()->ConsumeCtrlKey();
}

int OmniboxViewViews::GetTextWidth() const {
  // Returns the width necessary to display the current text, including any
  // necessary space for the cursor or border/margin.
  return GetRenderText()->GetContentWidth() + GetInsets().width();
}

bool OmniboxViewViews::IsImeComposing() const {
  return IsIMEComposing();
}

gfx::Size OmniboxViewViews::GetMinimumSize() const {
  const int kMinCharacters = 20;
  return gfx::Size(
      GetFontList().GetExpectedTextWidth(kMinCharacters) + GetInsets().width(),
      GetPreferredSize().height());
}

void OmniboxViewViews::OnPaint(gfx::Canvas* canvas) {
  if (latency_histogram_state_ == CHAR_TYPED) {
    DCHECK(!insert_char_time_.is_null());
    UMA_HISTOGRAM_TIMES("Omnibox.CharTypedToRepaintLatency.ToPaint",
                        base::TimeTicks::Now() - insert_char_time_);
    latency_histogram_state_ = ON_PAINT_CALLED;
  }

  {
    SCOPED_UMA_HISTOGRAM_TIMER("Omnibox.PaintTime");
    Textfield::OnPaint(canvas);
  }
}

void OmniboxViewViews::ExecuteCommand(int command_id, int event_flags) {
  // In the base class, touch text selection is deactivated when a command is
  // executed. Since we are not always calling the base class implementation
  // here, we need to deactivate touch text selection here, too.
  DestroyTouchSelection();
  switch (command_id) {
    // These commands don't invoke the popup via OnBefore/AfterPossibleChange().
    case IDS_PASTE_AND_GO:
      model()->PasteAndGo(GetClipboardText());
      return;
    case IDC_EDIT_SEARCH_ENGINES:
      location_bar_view_->command_updater()->ExecuteCommand(command_id);
      return;

    // These commands do invoke the popup.
    case IDS_APP_PASTE:
      ExecuteTextEditCommand(ui::TextEditCommand::PASTE);
      return;
    default:
      if (Textfield::IsCommandIdEnabled(command_id)) {
        // The Textfield code will invoke OnBefore/AfterPossibleChange() itself
        // as necessary.
        Textfield::ExecuteCommand(command_id, event_flags);
        return;
      }
      OnBeforePossibleChange();
      location_bar_view_->command_updater()->ExecuteCommand(command_id);
      OnAfterPossibleChange(true);
      return;
  }
}

ui::TextInputType OmniboxViewViews::GetTextInputType() const {
  ui::TextInputType input_type = views::Textfield::GetTextInputType();
  // We'd like to set the text input type to TEXT_INPUT_TYPE_URL, because this
  // triggers URL-specific layout in software keyboards, e.g. adding top-level
  // "/" and ".com" keys for English.  However, this also causes IMEs to default
  // to Latin character mode, which makes entering search queries difficult for
  // IME users. Therefore, we try to guess whether an IME will be used based on
  // the input language, and set the input type accordingly.
#if defined(OS_WIN)
  if (input_type != ui::TEXT_INPUT_TYPE_NONE && location_bar_view_) {
    ui::InputMethod* input_method =
        location_bar_view_->GetWidget()->GetInputMethod();
    if (input_method && input_method->IsInputLocaleCJK())
      return ui::TEXT_INPUT_TYPE_SEARCH;
  }
#endif
  return input_type;
}

void OmniboxViewViews::AddedToWidget() {
  views::Textfield::AddedToWidget();
  scoped_compositor_observer_.Add(GetWidget()->GetCompositor());
}

void OmniboxViewViews::RemovedFromWidget() {
  views::Textfield::RemovedFromWidget();
  scoped_compositor_observer_.RemoveAll();
}

bool OmniboxViewViews::ShouldDoLearning() {
  return location_bar_view_ && !location_bar_view_->profile()->IsOffTheRecord();
}

bool OmniboxViewViews::IsDropCursorForInsertion() const {
  // Dragging text from within omnibox itself will behave like text input
  // editor, showing insertion-style drop cursor as usual;
  // but dragging text from outside omnibox will replace entire contents with
  // paste-and-go behavior, so returning false in that case prevents the
  // confusing insertion-style drop cursor.
  return HasTextBeingDragged();
}

void OmniboxViewViews::SetTextAndSelectedRange(const base::string16& text,
                                               const gfx::Range& range) {
  SetText(text);
  SelectRange(range);
}

base::string16 OmniboxViewViews::GetSelectedText() const {
  // TODO(oshima): Support IME.
  return views::Textfield::GetSelectedText();
}

void OmniboxViewViews::OnPaste() {
  const base::string16 text(GetClipboardText());

  if (text.empty() ||
      // When the fakebox is focused, ignore pasted whitespace because if the
      // fakebox is hidden and there's only whitespace in the omnibox, it's
      // difficult for the user to see that the focus moved to the omnibox.
      (model()->focus_state() == OMNIBOX_FOCUS_INVISIBLE &&
       std::all_of(text.begin(), text.end(), base::IsUnicodeWhitespace))) {
    return;
  }

  OnBeforePossibleChange();
  // Record this paste, so we can do different behavior.
  model()->OnPaste();
  // Force a Paste operation to trigger the text_changed code in
  // OnAfterPossibleChange(), even if identical contents are pasted.
  state_before_change_.text.clear();
  InsertOrReplaceText(text);
  OnAfterPossibleChange(true);
}

bool OmniboxViewViews::HandleEarlyTabActions(const ui::KeyEvent& event) {
  // This must run before accelerator handling invokes a focus change on tab.
  // Note the parallel with SkipDefaultKeyEventProcessing above.
  if (!views::FocusManager::IsTabTraversalKeyEvent(event))
    return false;

  if (model()->is_keyword_hint() && !event.IsShiftDown())
    return model()->AcceptKeyword(KeywordModeEntryMethod::TAB);

  if (!model()->popup_model()->IsOpen())
    return false;

  if (event.IsShiftDown() && (model()->popup_model()->selected_line_state() ==
                              OmniboxPopupModel::KEYWORD)) {
    model()->ClearKeyword();
    return true;
  }

  // If tabbing forwards (shift is not pressed) and tab switch button is not
  // selected, select it.
  if (model()->popup_model()->SelectedLineHasTabMatch() &&
      model()->popup_model()->selected_line_state() ==
          OmniboxPopupModel::NORMAL &&
      !event.IsShiftDown()) {
    model()->popup_model()->SetSelectedLineState(OmniboxPopupModel::TAB_SWITCH);
    popup_view_->ProvideButtonFocusHint(
        model()->popup_model()->selected_line());
    return true;
  }

  // If tabbing backwards (shift is pressed), handle cases involving selecting
  // the tab switch button.
  if (event.IsShiftDown()) {
    // If tab switch button is focused, unfocus it.
    if (model()->popup_model()->selected_line_state() ==
        OmniboxPopupModel::TAB_SWITCH) {
      model()->popup_model()->SetSelectedLineState(OmniboxPopupModel::NORMAL);
      return true;
    }
    // Otherwise, if at top of results, do nothing.
    if (model()->popup_model()->selected_line() == 0)
      return false;
  }

  // Translate tab and shift-tab into down and up respectively.
  model()->OnUpOrDownKeyPressed(event.IsShiftDown() ? -1 : 1);
  // If we shift-tabbed (and actually moved) to a suggestion with a tab
  // switch button, select it.
  if (event.IsShiftDown() &&
      model()->popup_model()->SelectedLineHasTabMatch()) {
    model()->popup_model()->SetSelectedLineState(OmniboxPopupModel::TAB_SWITCH);
    popup_view_->ProvideButtonFocusHint(
        model()->popup_model()->selected_line());
  }

  return true;
}

void OmniboxViewViews::UpdateSecurityLevel() {
  security_level_ = controller()->GetToolbarModel()->GetSecurityLevel(false);
}

void OmniboxViewViews::SetWindowTextAndCaretPos(const base::string16& text,
                                                size_t caret_pos,
                                                bool update_popup,
                                                bool notify_text_changed) {
  const gfx::Range range(caret_pos, caret_pos);
  SetTextAndSelectedRange(text, range);

  if (update_popup)
    UpdatePopup();

  if (notify_text_changed)
    TextChanged();
}

void OmniboxViewViews::SetCaretPos(size_t caret_pos) {
  SelectRange(gfx::Range(caret_pos, caret_pos));
}

bool OmniboxViewViews::IsSelectAll() const {
  // TODO(oshima): IME support.
  return !text().empty() && text() == GetSelectedText();
}

void OmniboxViewViews::UpdatePopup() {
  // Prevent inline autocomplete when the caret isn't at the end of the text.
  const gfx::Range sel = GetSelectedRange();
  model()->UpdateInput(!sel.is_empty(), !SelectionAtEnd());
}

void OmniboxViewViews::ApplyCaretVisibility() {
  SetCursorEnabled(model()->is_caret_visible());

  // TODO(tommycli): Because the LocationBarView has a somewhat different look
  // depending on whether or not the caret is visible, we have to resend a
  // "focused" notification. Remove this once we get rid of the concept of
  // "invisible focus".
  if (location_bar_view_)
    location_bar_view_->OnOmniboxFocused();
}

void OmniboxViewViews::OnTemporaryTextMaybeChanged(
    const base::string16& display_text,
    const AutocompleteMatch& match,
    bool save_original_selection,
    bool notify_text_changed) {
  if (save_original_selection)
    saved_temporary_selection_ = GetSelectedRange();

  // Get friendly accessibility label.
  bool is_tab_switch_button_focused =
      model()->popup_model()->selected_line_state() ==
      OmniboxPopupModel::TAB_SWITCH;
  friendly_suggestion_text_ = AutocompleteMatchType::ToAccessibilityLabel(
      match, display_text, model()->popup_model()->selected_line(),
      model()->result().size(), is_tab_switch_button_focused,
      &friendly_suggestion_text_prefix_length_);
  SetWindowTextAndCaretPos(display_text, display_text.length(), false,
                           notify_text_changed);
#if defined(OS_MACOSX)
  // On macOS, the text field value changed notification is not
  // announced, so we need to explicitly announce the suggestion text.
  GetViewAccessibility().AnnounceText(friendly_suggestion_text_);
#endif
}

bool OmniboxViewViews::OnInlineAutocompleteTextMaybeChanged(
    const base::string16& display_text,
    size_t user_text_length) {
  if (display_text == text())
    return false;

  if (!IsIMEComposing()) {
    gfx::Range range(display_text.size(), user_text_length);
    SetTextAndSelectedRange(display_text, range);
  } else if (location_bar_view_) {
    location_bar_view_->SetImeInlineAutocompletion(
        display_text.substr(user_text_length));
  }
  TextChanged();
  return true;
}

void OmniboxViewViews::OnInlineAutocompleteTextCleared() {
  // Hide the inline autocompletion for IME users.
  if (location_bar_view_)
    location_bar_view_->SetImeInlineAutocompletion(base::string16());
}

void OmniboxViewViews::OnRevertTemporaryText() {
  SelectRange(saved_temporary_selection_);
  // We got here because the user hit the Escape key. We explicitly don't call
  // TextChanged(), since OmniboxPopupModel::ResetToDefaultMatch() has already
  // been called by now, and it would've called TextChanged() if it was
  // warranted.
}

void OmniboxViewViews::ClearAccessibilityLabel() {
  friendly_suggestion_text_.clear();
  friendly_suggestion_text_prefix_length_ = 0;
}

bool OmniboxViewViews::UnapplySteadyStateElisions(UnelisionGesture gesture) {
  // Early exit if no steady state elision features are enabled.
  if (!toolbar::features::IsHideSteadyStateUrlSchemeEnabled() &&
      !toolbar::features::IsHideSteadyStateUrlTrivialSubdomainsEnabled()) {
    return false;
  }

  // No need to update the text if the user is already inputting text.
  if (model()->user_input_in_progress())
    return false;

  // Don't unelide if we are currently displaying Query in Omnibox search terms,
  // as otherwise, it would be impossible to refine query terms.
  if (model()->GetQueryInOmniboxSearchTerms(nullptr /* search_terms */))
    return false;

  // If everything is selected, the user likely does not intend to edit the URL.
  // But if the Home key is pressed, the user probably does want to interact
  // with the beginning of the URL - in which case we unelide.
  if (IsSelectAll() && gesture != UnelisionGesture::HOME_KEY_PRESSED)
    return false;

  base::string16 full_url =
      controller()->GetToolbarModel()->GetFormattedFullURL();
  size_t start, end;
  GetSelectionBounds(&start, &end);

  // Find the length of the prefix that was chopped off to form the elided URL.
  // This simple logic only works because we elide only prefixes from the full
  // URL. Otherwise, we would have to use the FormatURL offset adjustments.
  size_t offset = full_url.find(GetText());
  if (offset != base::string16::npos) {
    if (start != end && gesture == UnelisionGesture::MOUSE_RELEASE &&
        !model()->ClassifiesAsSearch(GetSelectedText())) {
      // For user selections that look like a URL instead of a Search:
      // If we are uneliding at the end of a drag-select (on mouse release),
      // and the selection spans to the beginning of the elided URL, ensure that
      // the new selection spans to the beginning of the unelided URL too.
      // i.e. google.com/maps => https://www.google.com/maps
      //      ^^^^^^^^^^         ^^^^^^^^^^^^^^^^^^^^^^
      if (start != 0)
        start += offset;
      if (end != 0)
        end += offset;
    } else {
      start += offset;
      end += offset;
    }

    // Since we are changing the text in the double-click event handler, we
    // need to fix the cached indices of the double-clicked word.
    OffsetDoubleClickWord(offset);
  }

  // We have already early-exited if Query in Omnibox is active.
  model()->Unelide(false /* exit_query_in_omnibox */);
  SelectRange(gfx::Range(start, end));
  return true;
}

void OmniboxViewViews::OnBeforePossibleChange() {
  // Record our state.
  GetState(&state_before_change_);
  ime_composing_before_change_ = IsIMEComposing();

  // User is editing or traversing the text, as opposed to moving
  // through suggestions. Clear the accessibility label
  // so that the screen reader reports the raw text in the field.
  ClearAccessibilityLabel();
}

bool OmniboxViewViews::OnAfterPossibleChange(bool allow_keyword_ui_change) {
  // See if the text or selection have changed since OnBeforePossibleChange().
  State new_state;
  GetState(&new_state);
  OmniboxView::StateChanges state_changes =
      GetStateChanges(state_before_change_, new_state);

  state_changes.text_differs =
      state_changes.text_differs ||
      (ime_composing_before_change_ != IsIMEComposing());

  bool something_changed = model()->OnAfterPossibleChange(
      state_changes, allow_keyword_ui_change && !IsIMEComposing());

  // Unapply steady state elisions in response to selection changes due to
  // keystroke, tap gesture, and caret placement. Ignore selection changes while
  // the mouse is down, as we generally defer handling that until mouse release.
  if (state_changes.selection_differs && !is_mouse_pressed_ &&
      UnapplySteadyStateElisions(UnelisionGesture::OTHER)) {
    something_changed = true;
    state_changes.text_differs = true;
  }

  // If only selection was changed, we don't need to call model()'s
  // OnChanged() method, which is called in TextChanged().
  // But we still need to call EmphasizeURLComponents() to make sure the text
  // attributes are updated correctly.
  if (something_changed &&
      (state_changes.text_differs || state_changes.keyword_differs))
    TextChanged();
  else if (state_changes.selection_differs)
    EmphasizeURLComponents();

  return something_changed;
}

gfx::NativeView OmniboxViewViews::GetNativeView() const {
  return GetWidget()->GetNativeView();
}

gfx::NativeView OmniboxViewViews::GetRelativeWindowForPopup() const {
  return GetWidget()->GetTopLevelWidget()->GetNativeView();
}

int OmniboxViewViews::GetWidth() const {
  return location_bar_view_ ? location_bar_view_->width() : 0;
}

bool OmniboxViewViews::IsImeShowingPopup() const {
#if defined(OS_CHROMEOS)
  return ime_candidate_window_open_;
#else
  return GetInputMethod() ? GetInputMethod()->IsCandidatePopupOpen() : false;
#endif
}

void OmniboxViewViews::ShowVirtualKeyboardIfEnabled() {
  if (auto* input_method = GetInputMethod())
    input_method->ShowVirtualKeyboardIfEnabled();
}

void OmniboxViewViews::HideImeIfNeeded() {
  if (auto* input_method = GetInputMethod()) {
    if (auto* keyboard = input_method->GetInputMethodKeyboardController())
      keyboard->DismissVirtualKeyboard();
  }
}

int OmniboxViewViews::GetOmniboxTextLength() const {
  // TODO(oshima): Support IME.
  return static_cast<int>(text().length());
}

void OmniboxViewViews::SetEmphasis(bool emphasize, const gfx::Range& range) {
  SkColor color = location_bar_view_->GetColor(
      emphasize ? OmniboxPart::LOCATION_BAR_TEXT_DEFAULT
                : OmniboxPart::LOCATION_BAR_TEXT_DIMMED);
  if (range.IsValid())
    ApplyColor(color, range);
  else
    SetColor(color);
}

void OmniboxViewViews::UpdateSchemeStyle(const gfx::Range& range) {
  DCHECK(range.IsValid());
  // Only SECURE and DANGEROUS levels (pages served over HTTPS or flagged by
  // SafeBrowsing) get a special scheme color treatment. If the security level
  // is NONE or HTTP_SHOW_WARNING, we do not override the text style previously
  // applied to the scheme text range by SetEmphasis().
  if (security_level_ == security_state::NONE ||
      security_level_ == security_state::HTTP_SHOW_WARNING)
    return;
  ApplyColor(location_bar_view_->GetSecurityChipColor(security_level_), range);
  if (security_level_ == security_state::DANGEROUS)
    ApplyStyle(gfx::STRIKE, true, range);
}

void OmniboxViewViews::OnMouseMoved(const ui::MouseEvent& event) {
  if (location_bar_view_)
    location_bar_view_->OnOmniboxHovered(true);
}

void OmniboxViewViews::OnMouseExited(const ui::MouseEvent& event) {
  if (location_bar_view_)
    location_bar_view_->OnOmniboxHovered(false);
}

bool OmniboxViewViews::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDS_PASTE_AND_GO;
}

base::string16 OmniboxViewViews::GetLabelForCommandId(int command_id) const {
  DCHECK_EQ(IDS_PASTE_AND_GO, command_id);
  return l10n_util::GetStringUTF16(
      model()->ClassifiesAsSearch(GetClipboardText()) ? IDS_PASTE_AND_SEARCH
                                                      : IDS_PASTE_AND_GO);
}

const char* OmniboxViewViews::GetClassName() const {
  return kViewClassName;
}

bool OmniboxViewViews::OnMousePressed(const ui::MouseEvent& event) {
  is_mouse_pressed_ = true;

  select_all_on_mouse_release_ =
      (event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) &&
      (!HasFocus() || (model()->focus_state() == OMNIBOX_FOCUS_INVISIBLE));
  if (select_all_on_mouse_release_) {
    // Restore caret visibility whenever the user clicks in the omnibox in a way
    // that would give it focus.  We must handle this case separately here
    // because if the omnibox currently has invisible focus, the mouse event
    // won't trigger either SetFocus() or OmniboxEditModel::OnSetFocus().
    model()->SetCaretVisibility(true);

    // When we're going to select all on mouse release, invalidate any saved
    // selection lest restoring it fights with the "select all" action.  It's
    // possible to later set select_all_on_mouse_release_ back to false, but
    // that happens for things like dragging, which are cases where having
    // invalidated this saved selection is still OK.
    saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  }

  bool handled = views::Textfield::OnMousePressed(event);

  // This ensures that when the user makes a double-click partial select, we
  // perform the unelision at the same time as we make the partial selection,
  // which is on mousedown.
  if (!select_all_on_mouse_release_ &&
      UnapplySteadyStateElisions(UnelisionGesture::OTHER))
    TextChanged();

  return handled;
}

bool OmniboxViewViews::OnMouseDragged(const ui::MouseEvent& event) {
  if (HasTextBeingDragged())
    CloseOmniboxPopup();

  bool handled = views::Textfield::OnMouseDragged(event);

  if (HasSelection() || ExceededDragThreshold(event.root_location() -
                                              GetLastClickRootLocation())) {
    select_all_on_mouse_release_ = false;
  }

  return handled;
}

void OmniboxViewViews::OnMouseReleased(const ui::MouseEvent& event) {
  views::Textfield::OnMouseReleased(event);
  // When the user has clicked and released to give us focus, select all.
  if ((event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) &&
      select_all_on_mouse_release_) {
    // Select all in the reverse direction so as not to scroll the caret
    // into view and shift the contents jarringly.
    SelectAll(true);
  }
  select_all_on_mouse_release_ = false;

  // Make an unelision check on mouse release. This handles the drag selection
  // case, in which we defer uneliding until mouse release.
  is_mouse_pressed_ = false;
  if (UnapplySteadyStateElisions(UnelisionGesture::MOUSE_RELEASE))
    TextChanged();
}

void OmniboxViewViews::OnGestureEvent(ui::GestureEvent* event) {
  if (!HasFocus() && event->type() == ui::ET_GESTURE_TAP_DOWN) {
    select_all_on_gesture_tap_ = true;

    // If we're trying to select all on tap, invalidate any saved selection lest
    // restoring it fights with the "select all" action.
    saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  }

  views::Textfield::OnGestureEvent(event);

  if (select_all_on_gesture_tap_ && event->type() == ui::ET_GESTURE_TAP)
    SelectAll(true);

  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TAP_CANCEL ||
      event->type() == ui::ET_GESTURE_TWO_FINGER_TAP ||
      event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event->type() == ui::ET_GESTURE_PINCH_BEGIN ||
      event->type() == ui::ET_GESTURE_LONG_PRESS ||
      event->type() == ui::ET_GESTURE_LONG_TAP) {
    select_all_on_gesture_tap_ = false;
  }
}

void OmniboxViewViews::AboutToRequestFocusFromTabTraversal(bool reverse) {
  views::Textfield::AboutToRequestFocusFromTabTraversal(reverse);
}

bool OmniboxViewViews::SkipDefaultKeyEventProcessing(
    const ui::KeyEvent& event) {
  if (views::FocusManager::IsTabTraversalKeyEvent(event) &&
      ((model()->is_keyword_hint() && !event.IsShiftDown()) ||
       model()->popup_model()->IsOpen())) {
    return true;
  }
  if (event.key_code() == ui::VKEY_ESCAPE)
    return model()->WillHandleEscapeKey();
  return Textfield::SkipDefaultKeyEventProcessing(event);
}

void OmniboxViewViews::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTextField;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_LOCATION));
  node_data->AddStringAttribute(ax::mojom::StringAttribute::kAutoComplete,
                                "both");
// Expose keyboard shortcut where it makes sense.
#if defined(OS_MACOSX)
  // Use cloverleaf symbol for command key.
  node_data->AddStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts,
                                base::WideToUTF8(L"\u2318L"));
#else
  node_data->AddStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts,
                                "Ctrl+L");
#endif
  if (friendly_suggestion_text_.empty()) {
    // While user edits text, use the exact text displayed in the omnibox.
    node_data->SetValue(GetText());
  } else {
    // While user navigates omnibox suggestions, use the current editable
    // text decorated with additional friendly labelling text, such as the
    // title of the page and the type of autocomplete, for example:
    // "Google https://google.com location from history".
    // The edited text is always a substring of the friendly label, so that
    // users can navigate to specific characters in the friendly version using
    // Braille display routing keys or other assistive technologies.
    node_data->SetValue(friendly_suggestion_text_);
  }
  node_data->html_attributes.push_back(std::make_pair("type", "url"));

  // Establish a "CONTROLS" relationship between the omnibox and the
  // the popup. This allows a screen reader to understand the relationship
  // between the omnibox and the list of suggestions, and determine which
  // suggestion is currently selected, even though focus remains here on
  // the omnibox.
  int32_t popup_view_id =
      popup_view_->GetViewAccessibility().GetUniqueId().Get();
  node_data->AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                                 {popup_view_id});

  base::string16::size_type entry_start;
  base::string16::size_type entry_end;
  // Selection information is saved separately when focus is moved off the
  // current window - use that when there is no focus and it's valid.
  if (saved_selection_for_focus_change_.IsValid()) {
    entry_start = saved_selection_for_focus_change_.start();
    entry_end = saved_selection_for_focus_change_.end();
  } else {
    GetSelectionBounds(&entry_start, &entry_end);
  }
  node_data->AddIntAttribute(
      ax::mojom::IntAttribute::kTextSelStart,
      entry_start + friendly_suggestion_text_prefix_length_);
  node_data->AddIntAttribute(
      ax::mojom::IntAttribute::kTextSelEnd,
      entry_end + friendly_suggestion_text_prefix_length_);

  if (popup_window_mode_) {
    node_data->SetRestriction(ax::mojom::Restriction::kReadOnly);
  } else {
    node_data->AddState(ax::mojom::State::kEditable);
  }
}

bool OmniboxViewViews::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  if (read_only())
    return Textfield::HandleAccessibleAction(action_data);

  if (action_data.action == ax::mojom::Action::kSetValue) {
    SetUserText(base::UTF8ToUTF16(action_data.value), true);
    return true;
  } else if (action_data.action == ax::mojom::Action::kReplaceSelectedText) {
    model()->SetInputInProgress(true);
    if (saved_selection_for_focus_change_.IsValid()) {
      SelectRange(saved_selection_for_focus_change_);
      saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
    }
    InsertOrReplaceText(base::UTF8ToUTF16(action_data.value));
    TextChanged();
    return true;
  } else if (action_data.action == ax::mojom::Action::kSetSelection) {
    // Adjust for friendly text inserted at the start of the url.
    ui::AXActionData set_selection_action_data;
    set_selection_action_data.action = ax::mojom::Action::kSetSelection;
    set_selection_action_data.anchor_node_id = action_data.anchor_node_id;
    set_selection_action_data.focus_offset =
        action_data.focus_offset - friendly_suggestion_text_prefix_length_;
    set_selection_action_data.anchor_offset =
        action_data.anchor_offset - friendly_suggestion_text_prefix_length_;
    return Textfield::HandleAccessibleAction(set_selection_action_data);
  }

  return Textfield::HandleAccessibleAction(action_data);
}

void OmniboxViewViews::OnFocus() {
  views::Textfield::OnFocus();
  // TODO(tommycli): This does not seem like it should be necessary.
  // Investigate why it's needed and see if we can remove it.
  model()->ResetDisplayTexts();
  // TODO(oshima): Get control key state.
  model()->OnSetFocus(false);
  // Don't call controller()->OnSetFocus, this view has already acquired focus.

  // Restore the selection we saved in OnBlur() if it's still valid.
  if (saved_selection_for_focus_change_.IsValid()) {
    SelectRange(saved_selection_for_focus_change_);
    saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  }

  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);

  // Focus changes can affect the visibility of any keyword hint.
  if (location_bar_view_ && model()->is_keyword_hint())
    location_bar_view_->Layout();

// The user must be starting a session in the same tab as a previous one
// in order to display the new tab in-product help promo.
// While focusing the omnibox is not always a precursor to starting a new
// session, we don't want to wait until the user is in the middle of editing
// or navigating, because we'd like to show them the promo at the time when
// it would be immediately useful.
#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
  if (location_bar_view_ &&
      controller()->GetToolbarModel()->ShouldDisplayURL()) {
    feature_engagement::NewTabTrackerFactory::GetInstance()
        ->GetForProfile(location_bar_view_->profile())
        ->OnOmniboxFocused();
  }
#endif

  if (location_bar_view_)
    location_bar_view_->OnOmniboxFocused();
}

void OmniboxViewViews::OnBlur() {
  // Save the user's existing selection to restore it later.
  saved_selection_for_focus_change_ = GetSelectedRange();

  // Revert the URL if the user has not made any changes. If steady-state
  // elisions is on, this will also re-elide the URL.
  //
  // Because merely Alt-Tabbing to another window and back should not change the
  // Omnibox state, we only revert the text only if the Omnibox is blurred in
  // favor of some other View in the same Widget.
  if (GetWidget() && GetWidget()->IsActive() &&
      model()->user_input_in_progress() &&
      text() == model()->GetPermanentDisplayText()) {
    RevertAll();
  }

  views::Textfield::OnBlur();
  model()->OnWillKillFocus();

  // If ZeroSuggest is active, and there is evidence that there is a text
  // update to show, revert to ensure that update is shown now.  Otherwise,
  // at least call CloseOmniboxPopup(), so that if ZeroSuggest is in the
  // midst of running but hasn't yet opened the popup, it will be halted.
  // If we fully reverted in this case, we'd lose the cursor/highlight
  // information saved above. Note: popup_model() can be null in tests.
  //
  // TODO(tommycli): This seems like it should apply to the Cocoa version of
  // the Omnibox as well. Investigate moving this into the OmniboxEditModel.
  if (!model()->user_input_in_progress() && model()->popup_model() &&
      model()->popup_model()->IsOpen() &&
      text() != model()->GetPermanentDisplayText()) {
    RevertAll();
  } else {
    CloseOmniboxPopup();
  }

  // Tell the model to reset itself.
  model()->OnKillFocus();

  // Deselect the text. Ensures the cursor is an I-beam.
  SelectRange(gfx::Range(0));

  // When deselected, elide and reset scroll position. After eliding, the old
  // scroll offset is meaningless (since the string is guaranteed to fit within
  // the view). The scroll must be reset or the text may be rendered partly or
  // wholly off-screen.
  //
  // Important: Since the URL can contain bidirectional text, it is important to
  // set the display offset directly to 0 (not simply scroll to the start of the
  // text, since the start of the text may not be at the left edge).
  gfx::RenderText* render_text = GetRenderText();
  render_text->SetElideBehavior(gfx::ELIDE_TAIL);
  render_text->SetDisplayOffset(0);

  // Focus changes can affect the visibility of any keyword hint.
  // |location_bar_view_| can be null in tests.
  if (location_bar_view_) {
    if (model()->is_keyword_hint())
      location_bar_view_->Layout();

    location_bar_view_->OnOmniboxBlurred();

    // The location bar needs to repaint without a focus ring.
    location_bar_view_->SchedulePaint();
  }

  ClearAccessibilityLabel();
}

bool OmniboxViewViews::IsCommandIdEnabled(int command_id) const {
  if (command_id == IDS_APP_PASTE)
    return !read_only() && !GetClipboardText().empty();
  if (command_id == IDS_PASTE_AND_GO)
    return !read_only() && model()->CanPasteAndGo(GetClipboardText());
  return Textfield::IsCommandIdEnabled(command_id) ||
         location_bar_view_->command_updater()->IsCommandEnabled(command_id);
}

base::string16 OmniboxViewViews::GetSelectionClipboardText() const {
  return SanitizeTextForPaste(Textfield::GetSelectionClipboardText());
}

void OmniboxViewViews::DoInsertChar(base::char16 ch) {
  // When the fakebox is focused, ignore whitespace input because if the
  // fakebox is hidden and there's only whitespace in the omnibox, it's
  // difficult for the user to see that the focus moved to the omnibox.
  if ((model()->focus_state() == OMNIBOX_FOCUS_INVISIBLE) &&
      base::IsUnicodeWhitespace(ch)) {
    return;
  }

  // If |insert_char_time_| is not null, there's a pending insert char operation
  // that hasn't been painted yet. Keep the earlier time.
  if (insert_char_time_.is_null()) {
    DCHECK_EQ(latency_histogram_state_, NOT_ACTIVE);
    latency_histogram_state_ = CHAR_TYPED;
    insert_char_time_ = base::TimeTicks::Now();
  }
  Textfield::DoInsertChar(ch);
}

bool OmniboxViewViews::IsTextEditCommandEnabled(
    ui::TextEditCommand command) const {
  switch (command) {
    case ui::TextEditCommand::MOVE_UP:
    case ui::TextEditCommand::MOVE_DOWN:
      return !read_only();
    case ui::TextEditCommand::PASTE:
      return !read_only() && !GetClipboardText().empty();
    default:
      return Textfield::IsTextEditCommandEnabled(command);
  }
}

void OmniboxViewViews::ExecuteTextEditCommand(ui::TextEditCommand command) {
  // In the base class, touch text selection is deactivated when a command is
  // executed. Since we are not always calling the base class implementation
  // here, we need to deactivate touch text selection here, too.
  DestroyTouchSelection();

  if (!IsTextEditCommandEnabled(command))
    return;

  switch (command) {
    case ui::TextEditCommand::MOVE_UP:
      model()->OnUpOrDownKeyPressed(-1);
      break;
    case ui::TextEditCommand::MOVE_DOWN:
      model()->OnUpOrDownKeyPressed(1);
      break;
    case ui::TextEditCommand::PASTE:
      OnPaste();
      break;
    default:
      Textfield::ExecuteTextEditCommand(command);
      break;
  }
}

bool OmniboxViewViews::ShouldShowPlaceholderText() const {
  return Textfield::ShouldShowPlaceholderText() &&
         !model()->is_caret_visible() && !model()->is_keyword_selected();
}

#if defined(OS_CHROMEOS)
void OmniboxViewViews::CandidateWindowOpened(
      chromeos::input_method::InputMethodManager* manager) {
  ime_candidate_window_open_ = true;
}

void OmniboxViewViews::CandidateWindowClosed(
      chromeos::input_method::InputMethodManager* manager) {
  ime_candidate_window_open_ = false;
}
#endif

void OmniboxViewViews::ContentsChanged(views::Textfield* sender,
                                       const base::string16& new_contents) {
}

bool OmniboxViewViews::HandleKeyEvent(views::Textfield* textfield,
                                      const ui::KeyEvent& event) {
  if (event.type() == ui::ET_KEY_RELEASED) {
    // The omnibox contents may change while the control key is pressed.
    if (event.key_code() == ui::VKEY_CONTROL)
      model()->OnControlKeyChanged(false);

    return false;
  }

  // Skip processing of [Alt]+<num-pad digit> Unicode alt key codes.
  // Otherwise, if num-lock is off, the events are handled as [Up], [Down], etc.
  if (event.IsUnicodeKeyCode())
    return false;

  const bool shift = event.IsShiftDown();
  const bool control = event.IsControlDown();
  const bool alt = event.IsAltDown() || event.IsAltGrDown();
  const bool command = event.IsCommandDown();
  switch (event.key_code()) {
    case ui::VKEY_RETURN:
      if (model()->popup_model()->SelectedLineHasTabMatch() &&
          model()->popup_model()->selected_line_state() ==
              OmniboxPopupModel::TAB_SWITCH) {
        popup_view_->OpenMatch(WindowOpenDisposition::SWITCH_TO_TAB,
                               event.time_stamp());
      } else {
        if (alt || (shift && command)) {
          model()->AcceptInput(WindowOpenDisposition::NEW_FOREGROUND_TAB, false,
                               event.time_stamp());
        } else if (command) {
          model()->AcceptInput(WindowOpenDisposition::NEW_BACKGROUND_TAB, false,
                               event.time_stamp());
        } else if (shift) {
          model()->AcceptInput(WindowOpenDisposition::NEW_WINDOW, false,
                               event.time_stamp());
        } else {
          model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB, false,
                               event.time_stamp());
        }
      }
      return true;

    case ui::VKEY_ESCAPE:
      return model()->OnEscapeKeyPressed();

    case ui::VKEY_CONTROL:
      model()->OnControlKeyChanged(true);
      break;

    case ui::VKEY_DELETE:
      if (shift && model()->popup_model()->IsOpen())
        model()->popup_model()->TryDeletingCurrentItem();
      break;

    case ui::VKEY_UP:
      // Shift-up is handled by the text field class to enable text selection.
      if (shift)
        return false;

      if (IsTextEditCommandEnabled(ui::TextEditCommand::MOVE_UP)) {
        ExecuteTextEditCommand(ui::TextEditCommand::MOVE_UP);
        return true;
      }
      break;

    case ui::VKEY_DOWN:
      // Shift-down is handled by the text field class to enable text selection.
      if (shift)
        return false;

      if (IsTextEditCommandEnabled(ui::TextEditCommand::MOVE_DOWN)) {
        ExecuteTextEditCommand(ui::TextEditCommand::MOVE_DOWN);
        return true;
      }
      break;

    case ui::VKEY_PRIOR:
      if (control || alt || shift || read_only())
        return false;
      model()->OnUpOrDownKeyPressed(-1 * model()->result().size());
      return true;

    case ui::VKEY_NEXT:
      if (control || alt || shift || read_only())
        return false;
      model()->OnUpOrDownKeyPressed(model()->result().size());
      return true;

    case ui::VKEY_RIGHT:
      if (!(control || alt || shift)) {
        if (SelectionAtEnd() &&
            // Can be null in tests.
            model()->popup_model() &&
            model()->popup_model()->SelectedLineHasTabMatch()) {
          if (model()->popup_model()->selected_line_state() ==
              OmniboxPopupModel::NORMAL) {
            model()->popup_model()->SetSelectedLineState(
                OmniboxPopupModel::TAB_SWITCH);
            popup_view_->ProvideButtonFocusHint(
                model()->popup_model()->selected_line());
          }
          return true;
        }
      }
      break;

    case ui::VKEY_LEFT:
      if (!(control || alt || shift)) {
        if (SelectionAtEnd() &&
            // Can be null in tests.
            model()->popup_model() &&
            model()->popup_model()->SelectedLineHasTabMatch() &&
            model()->popup_model()->selected_line_state() ==
                OmniboxPopupModel::TAB_SWITCH) {
          model()->popup_model()->SetSelectedLineState(
              OmniboxPopupModel::NORMAL);
          return true;
        }
      }
      break;

    case ui::VKEY_V:
      if (control && !alt &&
          IsTextEditCommandEnabled(ui::TextEditCommand::PASTE)) {
        ExecuteTextEditCommand(ui::TextEditCommand::PASTE);
        return true;
      }
      break;

    case ui::VKEY_INSERT:
      if (shift && !control &&
          IsTextEditCommandEnabled(ui::TextEditCommand::PASTE)) {
        ExecuteTextEditCommand(ui::TextEditCommand::PASTE);
        return true;
      }
      break;

    case ui::VKEY_BACK:
      // No extra handling is needed in keyword search mode, if there is a
      // non-empty selection, or if the cursor is not leading the text.
      if (model()->is_keyword_hint() || model()->keyword().empty() ||
          HasSelection() || GetCursorPosition() != 0)
        return false;
      model()->ClearKeyword();
      return true;

    case ui::VKEY_HOME:
      // The Home key indicates that the user wants to move the cursor to the
      // beginning of the full URL, so it should always trigger an unelide.
      if (UnapplySteadyStateElisions(UnelisionGesture::HOME_KEY_PRESSED)) {
        if (shift) {
          // After uneliding, we need to move the end of the selection range
          // to the beginning of the full unelided URL.
          size_t start, end;
          GetSelectionBounds(&start, &end);
          SelectRange(gfx::Range(start, 0));
        } else {
          // After uneliding, move the caret to the beginning of the full
          // unelided URL.
          SetCaretPos(0);
        }

        TextChanged();
        return true;
      }
      break;

    case ui::VKEY_SPACE:
      if (!(control || alt || shift)) {
        if (SelectionAtEnd() &&
            model()->popup_model()->SelectedLineHasTabMatch() &&
            model()->popup_model()->selected_line_state() ==
                OmniboxPopupModel::TAB_SWITCH) {
          popup_view_->OpenMatch(WindowOpenDisposition::SWITCH_TO_TAB,
                                 event.time_stamp());
          return true;
        }
      }
      break;

    default:
      break;
  }

  return HandleEarlyTabActions(event);
}

void OmniboxViewViews::OnBeforeUserAction(views::Textfield* sender) {
  OnBeforePossibleChange();
}

void OmniboxViewViews::OnAfterUserAction(views::Textfield* sender) {
  OnAfterPossibleChange(true);
}

void OmniboxViewViews::OnAfterCutOrCopy(ui::ClipboardType clipboard_type) {
  ui::Clipboard* cb = ui::Clipboard::GetForCurrentThread();
  base::string16 selected_text;
  cb->ReadText(clipboard_type, &selected_text);
  GURL url;
  bool write_url;
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), &selected_text, &url,
                             &write_url);
  if (IsSelectAll())
    UMA_HISTOGRAM_COUNTS_1M(OmniboxEditModel::kCutOrCopyAllTextHistogram, 1);

  ui::ScopedClipboardWriter scoped_clipboard_writer(clipboard_type);
  scoped_clipboard_writer.WriteText(selected_text);
}

void OmniboxViewViews::OnWriteDragData(ui::OSExchangeData* data) {
  GURL url;
  bool write_url;
  base::string16 selected_text = GetSelectedText();
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), &selected_text, &url,
                             &write_url);
  data->SetString(selected_text);
  if (write_url) {
    gfx::Image favicon;
    base::string16 title = selected_text;
    if (IsSelectAll())
      model()->GetDataForURLExport(&url, &title, &favicon);
    button_drag_utils::SetURLAndDragImage(url, title, favicon.AsImageSkia(),
                                          nullptr, *GetWidget(), data);
    data->SetURL(url, title);
  }
}

void OmniboxViewViews::OnGetDragOperationsForTextfield(int* drag_operations) {
  base::string16 selected_text = GetSelectedText();
  GURL url;
  bool write_url;
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), &selected_text, &url,
                             &write_url);
  if (write_url)
    *drag_operations |= ui::DragDropTypes::DRAG_LINK;
}

void OmniboxViewViews::AppendDropFormats(
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  *formats = *formats | ui::OSExchangeData::URL;
}

int OmniboxViewViews::OnDrop(const ui::OSExchangeData& data) {
  if (HasTextBeingDragged())
    return ui::DragDropTypes::DRAG_NONE;

  base::string16 text;
  if (data.HasURL(ui::OSExchangeData::CONVERT_FILENAMES)) {
    GURL url;
    base::string16 title;
    if (data.GetURLAndTitle(ui::OSExchangeData::CONVERT_FILENAMES, &url,
                            &title)) {
      text = StripJavascriptSchemas(base::UTF8ToUTF16(url.spec()));
    }
  } else if (data.HasString() && data.GetString(&text)) {
    text = StripJavascriptSchemas(base::CollapseWhitespace(text, true));
  }

  if (text.empty())
    return ui::DragDropTypes::DRAG_NONE;

  SetUserText(text);
  if (!HasFocus())
    RequestFocus();
  SelectAll(false);
  return ui::DragDropTypes::DRAG_COPY;
}

void OmniboxViewViews::UpdateContextMenu(ui::SimpleMenuModel* menu_contents) {
  int paste_position = menu_contents->GetIndexOfCommandId(IDS_APP_PASTE);
  DCHECK_GE(paste_position, 0);
  menu_contents->InsertItemWithStringIdAt(
      paste_position + 1, IDS_PASTE_AND_GO, IDS_PASTE_AND_GO);

  menu_contents->AddSeparator(ui::NORMAL_SEPARATOR);

  // Minor note: We use IDC_ for command id here while the underlying textfield
  // is using IDS_ for all its command ids. This is because views cannot depend
  // on IDC_ for now.
  menu_contents->AddItemWithStringId(IDC_EDIT_SEARCH_ENGINES,
                                     IDS_EDIT_SEARCH_ENGINES);
}

void OmniboxViewViews::OnCompositingDidCommit(ui::Compositor* compositor) {
  if (latency_histogram_state_ == ON_PAINT_CALLED) {
    // Advance the state machine.
    latency_histogram_state_ = COMPOSITING_COMMIT;
  } else if (latency_histogram_state_ == COMPOSITING_COMMIT) {
    // If we get two commits in a row (without compositing end in-between), it
    // means compositing wasn't done for the previous commit, which can happen
    // due to occlusion. In such a case, reset the state to inactive and don't
    // log the metric.
    insert_char_time_ = base::TimeTicks();
    latency_histogram_state_ = NOT_ACTIVE;
  }
}

void OmniboxViewViews::OnCompositingStarted(ui::Compositor* compositor,
                                            base::TimeTicks start_time) {
  // Track the commit to completion. This state is necessary to ensure the ended
  // event we get is the one we're waiting for (and not for a previous paint).
  if (latency_histogram_state_ == COMPOSITING_COMMIT)
    latency_histogram_state_ = COMPOSITING_STARTED;
}

void OmniboxViewViews::OnCompositingEnded(ui::Compositor* compositor) {
  if (latency_histogram_state_ == COMPOSITING_STARTED) {
    DCHECK(!insert_char_time_.is_null());
    UMA_HISTOGRAM_TIMES("Omnibox.CharTypedToRepaintLatency",
                        base::TimeTicks::Now() - insert_char_time_);
    insert_char_time_ = base::TimeTicks();
    latency_histogram_state_ = NOT_ACTIVE;
  }
}

void OmniboxViewViews::OnCompositingChildResizing(ui::Compositor* compositor) {}

void OmniboxViewViews::OnCompositingShuttingDown(ui::Compositor* compositor) {
  scoped_compositor_observer_.RemoveAll();
}

void OmniboxViewViews::OnTemplateURLServiceChanged() {
  InstallPlaceholderText();
}
