// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include <memory>
#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_views.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_view_webui.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/grit/branded_strings.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_fixer.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ime/virtual_keyboard_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/selection_model.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/browser_process.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/display/screen.h"
#endif

namespace {

using ::metrics::OmniboxEventProto;
using ::ui::mojom::DragOperation;

// OmniboxState ---------------------------------------------------------------

// Stores omnibox state for each tab.
struct OmniboxState : public base::SupportsUserData::Data {
  static const char kKey[];

  OmniboxState(const OmniboxEditModel::State& model_state,
               const std::vector<gfx::Range> selection,
               const std::vector<gfx::Range> saved_selection_for_focus_change);

  ~OmniboxState() override;

  const OmniboxEditModel::State model_state;

  // We store both the actual selection and any saved selection (for when the
  // omnibox is not focused).  This allows us to properly handle cases like
  // selecting text, tabbing out of the omnibox, switching tabs away and back,
  // and tabbing back into the omnibox.
  const std::vector<gfx::Range> selection;
  const std::vector<gfx::Range> saved_selection_for_focus_change;
};

// static
const char OmniboxState::kKey[] = "OmniboxState";

OmniboxState::OmniboxState(
    const OmniboxEditModel::State& model_state,
    const std::vector<gfx::Range> selection,
    const std::vector<gfx::Range> saved_selection_for_focus_change)
    : model_state(model_state),
      selection(selection),
      saved_selection_for_focus_change(saved_selection_for_focus_change) {}

OmniboxState::~OmniboxState() = default;

bool IsClipboardDataMarkedAsConfidential() {
  return ui::Clipboard::GetForCurrentThread()
      ->IsMarkedByOriginatorAsConfidential();
}

}  // namespace

// OmniboxViewViews -----------------------------------------------------------

OmniboxViewViews::OmniboxViewViews(std::unique_ptr<OmniboxClient> client,
                                   bool popup_window_mode,
                                   LocationBarView* location_bar_view,
                                   const gfx::FontList& font_list)
    : OmniboxView(std::move(client)),
      popup_window_mode_(popup_window_mode),
      location_bar_view_(location_bar_view),
      latency_histogram_state_(NOT_ACTIVE),
      friendly_suggestion_text_prefix_length_(0) {
  SetID(VIEW_ID_OMNIBOX);
  SetProperty(views::kElementIdentifierKey, kOmniboxElementId);
  SetFontList(font_list);
  set_force_text_directionality(true);

  // Unit tests may use a mock location bar that has no browser,
  // or use no location bar at all.
  if (location_bar_view_ && location_bar_view_->browser()) {
    pref_change_registrar_.Init(
        location_bar_view_->browser()->profile()->GetPrefs());
    pref_change_registrar_.Add(
        omnibox::kPreventUrlElisionsInOmnibox,
        base::BindRepeating(&OmniboxViewViews::Update, base::Unretained(this)));
  }

  // Remove the default textfield hover effect. Omnibox has a custom hover
  // effect over the entire location bar.
  RemoveHoverEffect();

  GetViewAccessibility().SetRole(ax::mojom::Role::kTextField);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF8(IDS_ACCNAME_LOCATION));
  // Sometimes there are additional ignored views, such as a View representing
  // the cursor, inside the address bar's text field. These should always be
  // ignored by accessibility since a plain text field should always be a leaf
  // node in the accessibility trees of all the platforms we support.
  GetViewAccessibility().SetIsLeaf(true);
  if (popup_window_mode_) {
    GetViewAccessibility().SetReadOnly(true);
  } else {
    GetViewAccessibility().SetIsEditable(true);
  }
  GetViewAccessibility().SetAutoComplete("both");
  GetViewAccessibility().AddHTMLAttributes(std::make_pair("type", "url"));
  // Expose keyboard shortcut where it makes sense.
#if BUILDFLAG(IS_MAC)
  GetViewAccessibility().SetKeyShortcuts("⌘L");
#else
  GetViewAccessibility().SetKeyShortcuts("Ctrl+L");
#endif
}

OmniboxViewViews::~OmniboxViewViews() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::input_method::InputMethodManager::Get()->RemoveCandidateWindowObserver(
      this);
#endif

  // Explicitly teardown members which have a reference to us.  Just to be safe
  // we want them to be destroyed before destroying any other internal state.
  popup_view_.reset();
}

void OmniboxViewViews::Init() {
  set_controller(this);
  SetTextInputType(GetPreferredTextInputType());
  GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
  GetRenderText()->set_symmetric_selection_visual_bounds(true);
  InstallPlaceholderText();
  scoped_template_url_service_observation_.Observe(
      controller()->client()->GetTemplateURLService());

  if (popup_window_mode_) {
    SetReadOnly(true);
  }

  if (location_bar_view_) {
    if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup)) {
      popup_view_ = std::make_unique<OmniboxPopupViewWebUI>(
          /*omnibox_view=*/this, controller(), location_bar_view_);
    } else {
      popup_view_ = std::make_unique<OmniboxPopupViewViews>(
          /*omnibox_view=*/this, controller(), location_bar_view_);
    }
    popup_view_opened_subscription_ =
        popup_view_->AddOpenListener(base::BindRepeating(
            &OmniboxViewViews::OnPopupOpened, base::Unretained(this)));
    // Set whether the text should be used to improve typing suggestions.
    SetShouldDoLearning(!location_bar_view_->profile()->IsOffTheRecord());
  }

  // Override the default FocusableBorder from Textfield, since the
  // LocationBarView will indicate the focus state.
  constexpr gfx::Insets kTextfieldInsets(0);
  SetBorder(views::CreateEmptyBorder(kTextfieldInsets));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::input_method::InputMethodManager::Get()->AddCandidateWindowObserver(
      this);
#endif
}

void OmniboxViewViews::SaveStateToTab(content::WebContents* tab) {
  DCHECK(tab);

  // We don't want to keep the IME status, so force quit the current
  // session here.  It may affect the selection status, so order is
  // also important.
  if (IsIMEComposing()) {
    ConfirmCompositionText(/* keep_selection */ false);
    GetInputMethod()->CancelComposition(this);
  }

  // NOTE: GetStateForTabSwitch() may affect GetSelectedRange(), so order is
  // important.
  OmniboxEditModel::State state = model()->GetStateForTabSwitch();
  tab->SetUserData(
      OmniboxState::kKey,
      std::make_unique<OmniboxState>(state, GetRenderText()->GetAllSelections(),
                                     saved_selection_for_focus_change_));
}

void OmniboxViewViews::OnTabChanged(content::WebContents* web_contents) {
  const OmniboxState* state = static_cast<OmniboxState*>(
      web_contents->GetUserData(&OmniboxState::kKey));
  model()->RestoreState(state ? &state->model_state : nullptr);
  if (state) {
    // This assumes that the omnibox has already been focused or blurred as
    // appropriate; otherwise, a subsequent OnFocus() or OnBlur() call could
    // goof up the selection.  See comments on OnActiveTabChanged() call in
    // Browser::ActiveTabChanged().
    if (state->model_state.user_input_in_progress &&
        state->model_state.user_text.empty() &&
        state->model_state.keyword.empty()) {
      // See comment in OmniboxEditModel::GetStateForTabSwitch() for details on
      // this.
      SelectAll(true);
      saved_selection_for_focus_change_.clear();
    } else {
      SetSelectedRanges(state->selection);
      saved_selection_for_focus_change_ =
          state->saved_selection_for_focus_change;
    }
  }

  // TODO(msw|oshima): Consider saving/restoring edit history.
  ClearEditHistory();
}

void OmniboxViewViews::ResetTabState(content::WebContents* web_contents) {
  web_contents->SetUserData(OmniboxState::kKey, nullptr);
}

void OmniboxViewViews::InstallPlaceholderText() {
  // If `keyword_placeholder()` is set, then the user is in a keyword mode that
  // has placeholder text. Use that instead of the DSE placeholder text.
  if (!model()->keyword_placeholder().empty()) {
    SetPlaceholderText(model()->keyword_placeholder());
  } else if (const auto* default_provider = controller()
                                                ->client()
                                                ->GetTemplateURLService()
                                                ->GetDefaultSearchProvider()) {
    // Otherwise, if a DSE is set, use the DSE placeholder text.
    SetPlaceholderText(l10n_util::GetStringFUTF16(
        IDS_OMNIBOX_PLACEHOLDER_TEXT, default_provider->short_name()));
  } else {
    SetPlaceholderText(std::u16string());
  }

  UpdatePlaceholderTextColor();
}

bool OmniboxViewViews::GetSelectionAtEnd() const {
  const gfx::Range sel = GetSelectedRange();
  return sel.GetMin() == GetText().size();
}

void OmniboxViewViews::EmphasizeURLComponents() {
  // If the current contents is a URL, turn on special URL rendering mode in
  // RenderText.
  bool text_is_url = model()->CurrentTextIsURL();
  GetRenderText()->SetDirectionalityMode(
      text_is_url ? gfx::DIRECTIONALITY_AS_URL : gfx::DIRECTIONALITY_FROM_TEXT);
  SetStyle(gfx::TEXT_STYLE_STRIKE, false);

  std::u16string text = GetText();
  UpdateTextStyle(text, text_is_url,
                  controller()->client()->GetSchemeClassifier());
}

void OmniboxViewViews::Update() {
  if (model()->ResetDisplayTexts()) {
    RevertAll();

    // Only select all when we have focus.  If we don't have focus, selecting
    // all is unnecessary since the selection will change on regaining focus.
    if (model()->has_focus())
      SelectAll(true);
  } else {
    // If the text is unchanged, we still need to re-emphasize the text, as the
    // security state may be different from before the Update.
    EmphasizeURLComponents();
  }
}

std::u16string OmniboxViewViews::GetText() const {
  // TODO(oshima): IME support
  return Textfield::GetText();
}

void OmniboxViewViews::SetUserText(const std::u16string& text,
                                   bool update_popup) {
  saved_selection_for_focus_change_.clear();
  OmniboxView::SetUserText(text, update_popup);
}

void OmniboxViewViews::SetAdditionalText(
    const std::u16string& additional_text) {
  // TODO (manukh): Ideally, OmniboxView wouldn't be responsible for its sibling
  // label owned by LocationBarView. However, this is the only practical pathway
  // between the OmniboxEditModel, which handles setting the omnibox match, and
  // LocationBarView. Perhaps, if we decide to launch rich autocompletion we'll
  // consider alternatives.
  if (location_bar_view_)
    location_bar_view_->SetOmniboxAdditionalText(additional_text);
}

void OmniboxViewViews::EnterKeywordModeForDefaultSearchProvider() {
  // Transition the user into keyword mode using their default search provider.
  model()->EnterKeywordModeForDefaultSearchProvider(
      OmniboxEventProto::KEYBOARD_SHORTCUT);
}

void OmniboxViewViews::GetSelectionBounds(
    std::u16string::size_type* start,
    std::u16string::size_type* end) const {
  const gfx::Range range = GetSelectedRange();
  *start = static_cast<size_t>(range.start());
  *end = static_cast<size_t>(range.end());
}

size_t OmniboxViewViews::GetAllSelectionsLength() const {
  size_t sum = 0;
  for (auto s : GetRenderText()->GetAllSelections())
    sum += s.length();
  return sum;
}

void OmniboxViewViews::SelectAll(bool reversed) {
  views::Textfield::SelectAll(reversed);
}

void OmniboxViewViews::RevertAll() {
  saved_selection_for_focus_change_.clear();
  OmniboxView::RevertAll();
}

void OmniboxViewViews::SetFocus(bool is_user_initiated) {
  // Temporarily reveal the top-of-window views (if not already revealed) so
  // that the location bar view is visible and is considered focusable. When it
  // actually receives focus, ImmersiveFocusWatcher will add another lock to
  // keep it revealed. |location_bar_view_| can be nullptr in unit tests.
  std::unique_ptr<ImmersiveRevealedLock> focus_reveal_lock;
  if (location_bar_view_) {
    focus_reveal_lock =
        BrowserView::GetBrowserViewForBrowser(location_bar_view_->browser())
            ->immersive_mode_controller()
            ->GetRevealedLock(ImmersiveModeController::ANIMATE_REVEAL_YES);
  }

  const bool omnibox_already_focused = HasFocus();

  if (is_user_initiated)
    model()->Unelide();

  RequestFocus();

  if (omnibox_already_focused)
    model()->ClearKeyword();

  // If the user initiated the focus, then we always select-all, even if the
  // omnibox is already focused. This can happen if the user pressed Ctrl+L
  // while already typing in the omnibox.
  //
  // For renderer initiated focuses (like NTP or about:blank page load finish):
  //  - If the omnibox was not already focused, select-all. This handles the
  //    about:blank homepage case, where the location bar has initial focus.
  //    It annoys users if the URL is not pre-selected. https://crbug.com/45260.
  //  - If the omnibox is already focused, DO NOT select-all. This can happen
  //    if the user starts typing before the NTP finishes loading. If the NTP
  //    finishes loading and then does a renderer-initiated focus, performing
  //    a select-all here would surprisingly overwrite the user's first few
  //    typed characters. https://crbug.com/924935.
  if (is_user_initiated || !omnibox_already_focused)
    SelectAll(true);

  // |is_user_initiated| is true for focus events from keyboard accelerators.
  if (is_user_initiated)
    model()->StartZeroSuggestRequest();

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

int OmniboxViewViews::GetUnelidedTextWidth() const {
  auto elide_behavior = GetRenderText()->elide_behavior();
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);
  auto width = GetTextWidth();
  GetRenderText()->SetElideBehavior(elide_behavior);
  return width;
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
    auto now = base::TimeTicks::Now();
    UMA_HISTOGRAM_TIMES("Omnibox.CharTypedToRepaintLatency.ToPaint",
                        now - insert_char_time_);
    latency_histogram_state_ = ON_PAINT_CALLED;
    GetWidget()->GetCompositor()->RequestSuccessfulPresentationTimeForNextFrame(
        base::BindOnce(
            [](base::TimeTicks insert_timestamp,
               base::TimeTicks paint_timestamp,
               const viz::FrameTimingDetails& frame_timing_details) {
              base::TimeTicks presentation_timestamp =
                  frame_timing_details.presentation_feedback.timestamp;
              UMA_HISTOGRAM_TIMES(
                  "Omnibox.CharTypedToRepaintLatency.PaintToPresent",
                  presentation_timestamp - paint_timestamp);
              UMA_HISTOGRAM_TIMES(
                  "Omnibox.CharTypedToRepaintLatency.InsertToPresent",
                  presentation_timestamp - insert_timestamp);
            },
            insert_char_time_, now));
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
    case IDC_PASTE_AND_GO:
      model()->PasteAndGo(GetClipboardText(/*notify_if_restricted=*/true));
      return;
    case IDC_SHOW_FULL_URLS:
    case IDC_SHOW_GOOGLE_LENS_SHORTCUT:
    case IDC_EDIT_SEARCH_ENGINES:
      location_bar_view_->command_updater()->ExecuteCommand(command_id);
      return;

    case IDC_SEND_TAB_TO_SELF:
      send_tab_to_self::SendTabToSelfBubbleController::
          CreateOrGetFromWebContents(location_bar_view_->GetWebContents())
              ->ShowBubble();
      return;

    // These commands do invoke the popup.
    case Textfield::kPaste:
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

void OmniboxViewViews::OnInputMethodChanged() {
#if BUILDFLAG(IS_WIN)
  // Update the input type with the input method on Windows for CJK.
  SetTextInputType(GetPreferredTextInputType());
#endif  // BUILDFLAG(IS_WIN)
}

ui::TextInputType OmniboxViewViews::GetPreferredTextInputType() const {
#if BUILDFLAG(IS_WIN)
  // We'd like to set the text input type to TEXT_INPUT_TYPE_URL, because this
  // triggers URL-specific layout in software keyboards, e.g. adding top-level
  // "/" and ".com" keys for English.  However, this also causes IMEs to default
  // to Latin character mode, which makes entering search queries difficult for
  // IME users. Therefore, we try to guess whether an IME will be used based on
  // the input language, and set the input type accordingly.
  if (location_bar_view_) {
    ui::InputMethod* input_method =
        location_bar_view_->GetWidget()->GetInputMethod();
    if (input_method && input_method->IsInputLocaleCJK())
      return ui::TEXT_INPUT_TYPE_SEARCH;
  }
#endif  // BUILDFLAG(IS_WIN)
  return ui::TEXT_INPUT_TYPE_URL;
}

void OmniboxViewViews::AddedToWidget() {
  views::Textfield::AddedToWidget();
  scoped_compositor_observation_.Observe(GetWidget()->GetCompositor());
}

void OmniboxViewViews::RemovedFromWidget() {
  views::Textfield::RemovedFromWidget();
  scoped_compositor_observation_.Reset();
}

void OmniboxViewViews::UpdateSchemeStyle(const gfx::Range& range) {
  DCHECK(range.IsValid());
  DCHECK(!model()->user_input_in_progress());

  // Do not style the scheme for non-http/https URLs. For such schemes, styling
  // could be confusing or misleading. For example, the scheme isn't meaningful
  // in about:blank URLs. Or in blob: or filesystem: URLs, which have an inner
  // origin, the URL is likely too syntax-y to be able to meaningfully draw
  // attention to any part of it.
  if (!controller()->client()->GetNavigationEntryURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (net::IsCertStatusError(controller()->client()->GetCertStatus())) {
    if (location_bar_view_) {
      ApplyColor(location_bar_view_->GetSecurityChipColor(
                     controller()->client()->GetSecurityLevel()),
                 range);
    }
    ApplyStyle(gfx::TEXT_STYLE_STRIKE, true, range);
  }
}

void OmniboxViewViews::OnThemeChanged() {
  views::Textfield::OnThemeChanged();

  UpdatePlaceholderTextColor();
  SetSelectionBackgroundColor(
      GetColorProvider()->GetColor(kColorOmniboxSelectionBackground));
  SetSelectionTextColor(
      GetColorProvider()->GetColor(kColorOmniboxSelectionForeground));

  EmphasizeURLComponents();
}

bool OmniboxViewViews::IsDropCursorForInsertion() const {
  // Dragging text from within omnibox itself will behave like text input
  // editor, showing insertion-style drop cursor as usual;
  // but dragging text from outside omnibox will replace entire contents with
  // paste-and-go behavior, so returning false in that case prevents the
  // confusing insertion-style drop cursor.
  return HasTextBeingDragged();
}

void OmniboxViewViews::ApplyColor(SkColor color, const gfx::Range& range) {
  Textfield::ApplyColor(color, range);
}

void OmniboxViewViews::ApplyStyle(gfx::TextStyle style,
                                  bool value,
                                  const gfx::Range& range) {
  Textfield::ApplyStyle(style, value, range);
}

void OmniboxViewViews::SetTextAndSelectedRanges(
    const std::u16string& text,
    const std::vector<gfx::Range>& ranges) {
  DCHECK(!ranges.empty());

  // Will try to fit as much of the text preceding the cursor as possible. If
  // possible, guarantees at least |kPadLeading| chars of the text preceding the
  // the cursor are visible. If possible given the prior guarantee, also
  // guarantees |kPadTrailing| chars of the text following the cursor are
  // visible.
  static const size_t kPadTrailing = 30;
  static const size_t kPadLeading = 10;

  // We use SetTextWithoutCaretBoundsChangeNotification() in order to avoid
  // triggering accessibility events multiple times.
  SetTextWithoutCaretBoundsChangeNotification(text, ranges[0].end());
  Scroll({0, std::min(ranges[0].end() + kPadTrailing, text.size()),
          ranges[0].end() - std::min(kPadLeading, ranges[0].end())});
  // Setting the primary selected range will also fire an appropriate final
  // accessibility event after the changes above.
  SetSelectedRanges(ranges);

  // Clear the additional text.
  SetAdditionalText(std::u16string());
}

void OmniboxViewViews::SetSelectedRanges(
    const std::vector<gfx::Range>& ranges) {
  // Even when no text is selected, |ranges| should have at least 1 (empty)
  // Range representing the cursor.
  DCHECK(!ranges.empty());

  SetSelectedRange(ranges[0]);
  for (size_t i = 1; i < ranges.size(); i++)
    AddSecondarySelectedRange(ranges[i]);
}

std::u16string OmniboxViewViews::GetSelectedText() const {
  // TODO(oshima): Support IME.
  return views::Textfield::GetSelectedText();
}

void OmniboxViewViews::OnOmniboxPaste() {
  const std::u16string text(GetClipboardText(/*notify_if_restricted=*/true));

  if (text.empty() ||
      // When the fakebox is focused, ignore pasted whitespace because if the
      // fakebox is hidden and there's only whitespace in the omnibox, it's
      // difficult for the user to see that the focus moved to the omnibox.
      (model()->focus_state() == OMNIBOX_FOCUS_INVISIBLE &&
       base::ranges::all_of(text, base::IsUnicodeWhitespace<char16_t>))) {
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

  if (!model()->PopupIsOpen())
    return false;

  model()->OnTabPressed(event.IsShiftDown());

  return true;
}

#if BUILDFLAG(IS_MAC)
void OmniboxViewViews::AnnounceFriendlySuggestionText() {
  GetViewAccessibility().AnnounceText(friendly_suggestion_text_);
}
#endif

void OmniboxViewViews::SetWindowTextAndCaretPos(const std::u16string& text,
                                                size_t caret_pos,
                                                bool update_popup,
                                                bool notify_text_changed) {
  const gfx::Range range(caret_pos);
  SetTextAndSelectedRanges(text, {range});

  if (update_popup)
    UpdatePopup();

  if (notify_text_changed)
    TextChanged();
}

void OmniboxViewViews::SetCaretPos(size_t caret_pos) {
  SetSelectedRange(gfx::Range(caret_pos, caret_pos));
}

bool OmniboxViewViews::IsSelectAll() const {
  // TODO(oshima): IME support.
  return !GetText().empty() && GetText() == GetSelectedText();
}

void OmniboxViewViews::UpdatePopup() {
  // Prevent inline autocomplete when the caret isn't at the end of the text.
  const gfx::Range sel = GetSelectedRange();
  model()->UpdateInput(!sel.is_empty(), !GetSelectionAtEnd());
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
    const std::u16string& display_text,
    const AutocompleteMatch& match,
    bool save_original_selection,
    bool notify_text_changed) {
  if (save_original_selection)
    saved_temporary_selection_ = GetRenderText()->GetAllSelections();

  // SetWindowTextAndCaretPos will fire the accessibility notification,
  // so do not also generate redundant notification here.
  SetAccessibilityLabel(display_text, match, false);

  SetWindowTextAndCaretPos(display_text, display_text.length(), false,
                           notify_text_changed);
}

void OmniboxViewViews::OnInlineAutocompleteTextMaybeChanged(
    const std::u16string& display_text,
    std::vector<gfx::Range> selections,
    const std::u16string& prefix_autocompletion,
    const std::u16string& inline_autocompletion) {
  if (display_text == GetText())
    return;

  if (!IsIMEComposing()) {
    SetTextAndSelectedRanges(display_text, selections);
  } else if (location_bar_view_) {
    location_bar_view_->SetImePrefixAutocompletion(prefix_autocompletion);
    location_bar_view_->SetImeInlineAutocompletion(inline_autocompletion);
  }

  EmphasizeURLComponents();
}

void OmniboxViewViews::OnInlineAutocompleteTextCleared() {
  // Hide the inline autocompletion for IME users.
  if (location_bar_view_) {
    location_bar_view_->SetImePrefixAutocompletion(std::u16string());
    location_bar_view_->SetImeInlineAutocompletion(std::u16string());
  }
}

void OmniboxViewViews::OnRevertTemporaryText(const std::u16string& display_text,
                                             const AutocompleteMatch& match) {
  // We got here because the user hit the Escape key. We explicitly don't call
  // TextChanged(), since OmniboxPopupModel::ResetToDefaultMatch() has already
  // been called by now, and it would've called TextChanged() if it was
  // warranted.
  // However, it's important to notify accessibility that the value has changed,
  // otherwise the screen reader will use the old accessibility label text.
  SetAccessibilityLabel(display_text, match, true);
  SetSelectedRanges(saved_temporary_selection_);
}

void OmniboxViewViews::ClearAccessibilityLabel() {
  if (friendly_suggestion_text_.empty())
    return;
  friendly_suggestion_text_.clear();
  friendly_suggestion_text_prefix_length_ = 0;

  UpdateAccessibleValue();
}

void OmniboxViewViews::SetAccessibilityLabel(const std::u16string& display_text,
                                             const AutocompleteMatch& match,
                                             bool notify_text_changed) {
  if (model()->GetPopupSelection().line == OmniboxPopupSelection::kNoMatch) {
    // If nothing is selected in the popup, we are in the no-default-match edge
    // case, and |match| is a synthetically generated match. In that case,
    // bypass OmniboxPopupModel and get the label from our synthetic |match|.
    friendly_suggestion_text_ = AutocompleteMatchType::ToAccessibilityLabel(
        match, display_text, OmniboxPopupSelection::kNoMatch,
        controller()->autocomplete_controller()->result().size(),
        std::u16string(), &friendly_suggestion_text_prefix_length_);
  } else {
    friendly_suggestion_text_ =
        model()->GetPopupAccessibilityLabelForCurrentSelection(
            display_text, true, &friendly_suggestion_text_prefix_length_);

    // If the line immediately after the current selection is the
    // informational IPH row, append its accessibility label at the end of
    // this selection's accessibility label.
    friendly_suggestion_text_ +=
        model()->MaybeGetPopupAccessibilityLabelForIPHSuggestion();
  }

  UpdateAccessibleValue();

#if BUILDFLAG(IS_MAC)
  // On macOS, the only way to get VoiceOver to speak the friendly suggestion
  // text (for example, "how to open a pdf, search suggestion, 4 of 8") is
  // with an explicit announcement. Use PostTask to ensure that this
  // announcement happens after the text change notification, otherwise
  // the text change can interrupt the announcement.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OmniboxViewViews::AnnounceFriendlySuggestionText,
                     weak_factory_.GetWeakPtr()));
#endif
}

bool OmniboxViewViews::UnapplySteadyStateElisions(UnelisionGesture gesture) {
  // If everything is selected, the user likely does not intend to edit the URL.
  // But if the Home key is pressed, the user probably does want to interact
  // with the beginning of the URL - in which case we unelide.
  if (IsSelectAll() && gesture != UnelisionGesture::HOME_KEY_PRESSED)
    return false;

  // Get the original selection bounds so we can adjust it later.
  size_t start, end;
  GetSelectionBounds(&start, &end);

  // Try to unelide. Early exit if there's no unelisions to perform.
  std::u16string original_text = GetText();
  std::u16string original_selected_text = GetSelectedText();
  if (!model()->Unelide())
    return false;

  // Find the length of the prefix that was chopped off to form the elided URL.
  // This simple logic only works because we elide only prefixes from the full
  // URL. Otherwise, we would have to use the FormatURL offset adjustments.
  size_t offset = GetText().find(original_text);

  // Some intranet URLs have an elided form that's not a substring of the full
  // URL string. e.g. "https://foobar" has the elided form "foobar/". This is
  // to prevent elided URLs from looking like search terms. See
  // AutocompleteInput::FormattedStringWithEquivalentMeaning for details.
  //
  // In this special case, chop off the trailing slash and search again.
  if (offset == std::u16string::npos && !original_text.empty() &&
      original_text.back() == u'/') {
    offset = GetText().find(original_text.substr(0, original_text.size() - 1));
  }

  if (offset != std::u16string::npos) {
    AutocompleteMatch match;
    model()->ClassifyString(original_selected_text, &match, nullptr);
    bool selection_classifes_as_search =
        AutocompleteMatch::IsSearchType(match.type);
    if (start != end && gesture == UnelisionGesture::MOUSE_RELEASE &&
        !selection_classifes_as_search) {
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

  SetSelectedRange(gfx::Range(start, end));
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

void OmniboxViewViews::OnKeywordPlaceholderTextChange() {
  InstallPlaceholderText();
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return ime_candidate_window_open_;
#else
  return GetInputMethod() ? GetInputMethod()->IsCandidatePopupOpen() : false;
#endif
}

void OmniboxViewViews::ShowVirtualKeyboardIfEnabled() {
  if (auto* input_method = GetInputMethod())
    input_method->SetVirtualKeyboardVisibilityIfEnabled(true);
}

void OmniboxViewViews::HideImeIfNeeded() {
  if (auto* input_method = GetInputMethod()) {
    if (auto* keyboard = input_method->GetVirtualKeyboardController())
      keyboard->DismissVirtualKeyboard();
  }
}

int OmniboxViewViews::GetOmniboxTextLength() const {
  // TODO(oshima): Support IME.
  return static_cast<int>(GetText().length());
}

void OmniboxViewViews::SetEmphasis(bool emphasize, const gfx::Range& range) {
  const SkColor color = GetColorProvider()->GetColor(
      emphasize ? kColorOmniboxText : kColorOmniboxTextDimmed);
  if (range.IsValid())
    ApplyColor(color, range);
  else
    SetColor(color);
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
  return command_id == IDC_PASTE_AND_GO;
}

std::u16string OmniboxViewViews::GetLabelForCommandId(int command_id) const {
  DCHECK_EQ(IDC_PASTE_AND_GO, command_id);

  // Don't paste-and-go data that was marked by its originator as confidential.
  constexpr size_t kMaxSelectionTextLength = 50;
  const std::u16string clipboard_text =
      IsClipboardDataMarkedAsConfidential()
          ? std::u16string()
          : GetClipboardText(/*notify_if_restricted=*/false);

  if (clipboard_text.empty())
    return l10n_util::GetStringUTF16(IDS_PASTE_AND_GO_EMPTY);

  std::u16string selection_text = gfx::TruncateString(
      clipboard_text, kMaxSelectionTextLength, gfx::WORD_BREAK);

  AutocompleteMatch match;
  model()->ClassifyString(clipboard_text, &match, nullptr);
  if (AutocompleteMatch::IsSearchType(match.type))
    return l10n_util::GetStringFUTF16(IDS_PASTE_AND_SEARCH, selection_text);

  // To ensure the search and url strings began to truncate at the exact same
  // number of characters, the pixel width at which the url begins to elide is
  // derived from the truncated selection text. However, ideally there would be
  // a better way to do this.
  const float kMaxSelectionPixelWidth =
      GetStringWidthF(selection_text, Textfield::GetFontList());
  std::u16string url = url_formatter::ElideUrl(
      match.destination_url, Textfield::GetFontList(), kMaxSelectionPixelWidth);

  return l10n_util::GetStringFUTF16(IDS_PASTE_AND_GO, url);
}

bool OmniboxViewViews::OnMousePressed(const ui::MouseEvent& event) {
  PermitExternalProtocolHandler();

  // Clear focus of buttons, but do not clear keyword mode.
  if (model()->PopupIsOpen()) {
    OmniboxPopupSelection selection = model()->GetPopupSelection();
    if (selection.state != OmniboxPopupSelection::KEYWORD_MODE) {
      selection.state = OmniboxPopupSelection::NORMAL;
      model()->SetPopupSelection(selection);
    }
  }

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
    saved_selection_for_focus_change_.clear();
  }

  // Show on-focus suggestions if either:
  //  - The textfield doesn't already have focus.
  //  - Or if the textfield is empty, to cover the NTP ZeroSuggest case.
  if (event.IsOnlyLeftMouseButton() && (!HasFocus() || GetText().empty()))
    model()->StartZeroSuggestRequest();

  bool handled = views::Textfield::OnMousePressed(event);

  // Reset next double click length
  if (event.GetClickCount() == 1)
    next_double_click_selection_len_ = 0;

  if (!select_all_on_mouse_release_) {
    if (UnapplySteadyStateElisions(UnelisionGesture::OTHER)) {
      // This ensures that when the user makes a double-click partial select, we
      // perform the unelision at the same time as we make the partial
      // selection, which is on mousedown.
      TextChanged();
      filter_drag_events_for_unelision_ = true;
    } else if (event.GetClickCount() == 1 && event.IsLeftMouseButton()) {
      // Select the current word and record it for later. This is done to handle
      // an edge case where the wrong word is selected on a double click when
      // the elided URL is selected prior to the double click. Unelision happens
      // between the first and second click, causing the wrong word to be
      // selected because it's based on the click position in the newly unelided
      // URL. See https://crbug.com/1084406.
      if (IsSelectAll()) {
        SelectWordAt(event.location());
        std::u16string shown_url = GetText();
        std::u16string full_url = controller()->client()->GetFormattedFullURL();
        size_t offset = full_url.find(shown_url);
        if (offset != std::u16string::npos) {
          next_double_click_selection_len_ = GetSelectedText().length();
          next_double_click_selection_offset_ =
              offset + GetCursorPosition() - next_double_click_selection_len_;
        }
        // Reset selection
        // Select all in the reverse direction so as not to scroll the caret
        // into view and shift the contents jarringly.
        SelectAll(true);
      }
    } else if (event.GetClickCount() == 2 && event.IsLeftMouseButton()) {
      // If the user double clicked and we unelided between the first and second
      // click, offset double click.
      if (next_double_click_selection_len_ != 0) {
        SetSelectedRange(gfx::Range(next_double_click_selection_offset_,
                                    next_double_click_selection_offset_ +
                                        next_double_click_selection_len_));
      }
    }
  }

  return handled;
}

bool OmniboxViewViews::OnMouseDragged(const ui::MouseEvent& event) {
  if (filter_drag_events_for_unelision_ &&
      !ExceededDragThreshold(event.root_location() -
                             GetLastClickRootLocation())) {
    return true;
  }

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
  PermitExternalProtocolHandler();

  views::Textfield::OnMouseReleased(event);
  // When the user has clicked and released to give us focus, select all.
  if ((event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) &&
      select_all_on_mouse_release_) {
    // Select all in the reverse direction so as not to scroll the caret
    // into view and shift the contents jarringly.
    SelectAll(true);
  }
  select_all_on_mouse_release_ = false;

  is_mouse_pressed_ = false;
  filter_drag_events_for_unelision_ = false;

  // Make an unelision check on mouse release. This handles the drag selection
  // case, in which we defer uneliding until mouse release.
  if (UnapplySteadyStateElisions(UnelisionGesture::MOUSE_RELEASE))
    TextChanged();
}

void OmniboxViewViews::OnGestureEvent(ui::GestureEvent* event) {
  PermitExternalProtocolHandler();

  const bool gesture_should_take_focus =
      !HasFocus() && event->type() == ui::EventType::kGestureTap;
  if (gesture_should_take_focus) {
    select_all_on_gesture_tap_ = true;

    // If we're trying to select all on tap, invalidate any saved selection lest
    // restoring it fights with the "select all" action.
    saved_selection_for_focus_change_.clear();
  }

  // Show on-focus suggestions if either:
  //  - The textfield is taking focus.
  //  - The textfield is focused but empty, to cover the NTP ZeroSuggest case.
  if (gesture_should_take_focus || (HasFocus() && GetText().empty()))
    model()->StartZeroSuggestRequest();

  views::Textfield::OnGestureEvent(event);

  if (select_all_on_gesture_tap_ &&
      event->type() == ui::EventType::kGestureTap) {
    // Select all in the reverse direction so as not to scroll the caret
    // into view and shift the contents jarringly.
    SelectAll(true);
  }

  if (event->type() == ui::EventType::kGestureTap ||
      event->type() == ui::EventType::kGestureTapCancel ||
      event->type() == ui::EventType::kGestureTwoFingerTap ||
      event->type() == ui::EventType::kGestureScrollBegin ||
      event->type() == ui::EventType::kGesturePinchBegin ||
      event->type() == ui::EventType::kGestureLongPress ||
      event->type() == ui::EventType::kGestureLongTap) {
    select_all_on_gesture_tap_ = false;
  }
}

bool OmniboxViewViews::SkipDefaultKeyEventProcessing(
    const ui::KeyEvent& event) {
  if (views::FocusManager::IsTabTraversalKeyEvent(event) &&
      ((model()->is_keyword_hint() && !event.IsShiftDown()) ||
       model()->PopupIsOpen())) {
    return true;
  }
  if (event.key_code() == ui::VKEY_ESCAPE && !event.IsShiftDown()) {
    return true;
  }
  return Textfield::SkipDefaultKeyEventProcessing(event);
}

void OmniboxViewViews::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Textfield::GetAccessibleNodeData(node_data);

  if (model()->PopupIsOpen()) {
    popup_view_->AddPopupAccessibleNodeData(node_data);
  }

  std::u16string::size_type entry_start;
  std::u16string::size_type entry_end;
  // Selection information is saved separately when focus is moved off the
  // current window - use that when there is no focus and it's valid.
  if (!saved_selection_for_focus_change_.empty()) {
    entry_start = saved_selection_for_focus_change_[0].start();
    entry_end = saved_selection_for_focus_change_[0].end();
  } else {
    GetSelectionBounds(&entry_start, &entry_end);
  }
  node_data->AddIntAttribute(
      ax::mojom::IntAttribute::kTextSelStart,
      entry_start + friendly_suggestion_text_prefix_length_);
  node_data->AddIntAttribute(
      ax::mojom::IntAttribute::kTextSelEnd,
      entry_end + friendly_suggestion_text_prefix_length_);
}

bool OmniboxViewViews::HandleAccessibleAction(
    const ui::AXActionData& action_data) {
  if (GetReadOnly())
    return Textfield::HandleAccessibleAction(action_data);

  if (action_data.action == ax::mojom::Action::kSetValue) {
    SetUserText(base::UTF8ToUTF16(action_data.value), true);
    return true;
  } else if (action_data.action == ax::mojom::Action::kReplaceSelectedText) {
    model()->SetInputInProgress(true);
    if (!saved_selection_for_focus_change_.empty()) {
      SetSelectedRanges(saved_selection_for_focus_change_);
      saved_selection_for_focus_change_.clear();
    }
    InsertOrReplaceText(base::UTF8ToUTF16(action_data.value));
    TextChanged();
    return true;
  } else if (action_data.action == ax::mojom::Action::kSetSelection) {
    // Adjust for friendly text inserted at the start of the url.
    ui::AXActionData set_selection_action_data;
    set_selection_action_data.action = ax::mojom::Action::kSetSelection;
    set_selection_action_data.anchor_node_id = action_data.anchor_node_id;
    set_selection_action_data.focus_node_id = action_data.focus_node_id;
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
  // Don't call WebLocationBar::OnSetFocus(), this view has already acquired
  // focus.

  // Restore the selection we saved in OnBlur() if it's still valid.
  if (!saved_selection_for_focus_change_.empty()) {
    SetSelectedRanges(saved_selection_for_focus_change_);
    saved_selection_for_focus_change_.clear();
  }

  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
  // The text offsets are no longer valid when the elide behavior changes.
  SetNeedsAccessibleTextOffsetsUpdate();
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

  if (location_bar_view_)
    location_bar_view_->OnOmniboxFocused();
}

void OmniboxViewViews::OnBlur() {
  // Save the user's existing selection to restore it later.
  saved_selection_for_focus_change_ = GetRenderText()->GetAllSelections();

  // If the view is showing text that's not user-text, revert the text to the
  // permanent display text. This usually occurs if Steady State Elisions is on
  // and the user has unelided, but not edited the URL.
  //
  // Because merely Alt-Tabbing to another window and back should not change the
  // Omnibox state, we only revert the text only if the Omnibox is blurred in
  // favor of some other View in the same Widget.
  //
  // Also revert if the text has been edited but currently exactly matches
  // the permanent text. An example of this scenario is someone typing on the
  // new tab page and then deleting everything using backspace/delete.
  //
  // This should never exit keyword mode.
  if (GetWidget() && GetWidget()->IsActive() &&
      !model()->is_keyword_selected() &&
      ((!model()->user_input_in_progress() &&
        GetText() != model()->GetPermanentDisplayText()) ||
       (model()->user_input_in_progress() &&
        GetText() == model()->GetPermanentDisplayText()))) {
    RevertAll();
  }

  views::Textfield::OnBlur();
  model()->OnWillKillFocus();

  // If ZeroSuggest is active, and there is evidence that there is a text
  // update to show, revert to ensure that update is shown now.  Otherwise,
  // at least call CloseOmniboxPopup(), so that if ZeroSuggest is in the
  // midst of running but hasn't yet opened the popup, it will be halted.
  // If we fully reverted in this case, we'd lose the cursor/highlight
  // information saved above.
  if (!model()->user_input_in_progress() && model()->PopupIsOpen() &&
      GetText() != model()->GetPermanentDisplayText()) {
    RevertAll();
  } else {
    CloseOmniboxPopup();
  }

  // Tell the model to reset itself.
  model()->OnKillFocus();

  // Deselect the text. Ensures the cursor is an I-beam.
  SetSelectedRange(gfx::Range(0));

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

#if BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)
  // The text offsets are no longer valid when the elide behavior changes.
  SetNeedsAccessibleTextOffsetsUpdate();
#endif  // BUILDFLAG(SUPPORTS_AX_TEXT_OFFSETS)

  // In cases where there's a lot of whitespace in the text being shown, we want
  // the elision marker to be at the right of the text field, so don't elide
  // whitespace to the left of the elision point.
  render_text->SetWhitespaceElision(false);
  render_text->SetDisplayOffset(0);

  // |location_bar_view_| can be null in tests.
  if (location_bar_view_) {
    location_bar_view_->OnOmniboxBlurred();

    // The location bar needs to repaint without a focus ring.
    location_bar_view_->SchedulePaint();
  }

  ClearAccessibilityLabel();
}

bool OmniboxViewViews::IsCommandIdEnabled(int command_id) const {
  if (command_id == Textfield::kPaste)
    return !GetReadOnly() &&
           !GetClipboardText(/*notify_if_restricted=*/false).empty();
  if (command_id == IDC_PASTE_AND_GO) {
    return !GetReadOnly() && !IsClipboardDataMarkedAsConfidential() &&
           model()->CanPasteAndGo(
               GetClipboardText(/*notify_if_restricted=*/false));
  }

  // These menu items are only shown when they are valid.
  if (command_id == IDC_SHOW_FULL_URLS ||
      command_id == IDC_SHOW_GOOGLE_LENS_SHORTCUT) {
    return true;
  }

  return Textfield::IsCommandIdEnabled(command_id) ||
         (location_bar_view_ &&
          location_bar_view_->command_updater()->IsCommandEnabled(command_id));
}

OmniboxPopupView* OmniboxViewViews::GetPopupViewForTesting() const {
  return popup_view_.get();
}

std::u16string OmniboxViewViews::GetSelectionClipboardText() const {
  return SanitizeTextForPaste(Textfield::GetSelectionClipboardText());
}

void OmniboxViewViews::DoInsertChar(char16_t ch) {
  // Note: Using `Textfield::GetText()` instead of the `OmniboxView`
  // implementation because the latter makes full string copies of the former.
  if (model()->MaybeAccelerateKeywordSelection(Textfield::GetText(), ch)) {
    return;
  }

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
      return !GetReadOnly();
    case ui::TextEditCommand::PASTE:
      return !GetReadOnly() &&
             !GetClipboardText(show_rejection_ui_if_any_).empty();
    default:
      return Textfield::IsTextEditCommandEnabled(command);
  }
}

void OmniboxViewViews::ExecuteTextEditCommand(ui::TextEditCommand command) {
  // In the base class, touch text selection is deactivated when a command is
  // executed. Since we are not always calling the base class implementation
  // here, we need to deactivate touch text selection here, too.
  DestroyTouchSelection();

  base::AutoReset<bool> show_rejection_ui(&show_rejection_ui_if_any_, true);

  if (!IsTextEditCommandEnabled(command))
    return;

  switch (command) {
    case ui::TextEditCommand::MOVE_UP:
      model()->OnUpOrDownPressed(false, false);
      break;
    case ui::TextEditCommand::MOVE_DOWN:
      model()->OnUpOrDownPressed(true, false);
      break;
    case ui::TextEditCommand::PASTE:
      OnOmniboxPaste();
      break;
    default:
      Textfield::ExecuteTextEditCommand(command);
      break;
  }
}

bool OmniboxViewViews::ShouldShowPlaceholderText() const {
  // The DSE placeholder text is visible only if the omnibox is blurred. The
  // keyword placeholder text is visible even if the omnibox is focused, because
  // users won't enter keyword mode, blur the omnibox, read the placeholder
  // text, refocus the omnibox, and begin typing.
  return Textfield::ShouldShowPlaceholderText() &&
         (!model()->is_caret_visible() ||
          !model()->keyword_placeholder().empty());
}

void OmniboxViewViews::UpdateAccessibleValue() {
  if (friendly_suggestion_text_.empty()) {
    // While user edits text, use the exact text displayed in the omnibox.
    GetViewAccessibility().SetValue(GetText());
  } else {
    // While user navigates omnibox suggestions, use the current editable
    // text decorated with additional friendly labelling text, such as the
    // title of the page and the type of autocomplete, for example:
    // "Google https://google.com location from history".
    // The edited text is always a substring of the friendly label, so that
    // users can navigate to specific characters in the friendly version using
    // Braille display routing keys or other assistive technologies.
    GetViewAccessibility().SetValue(friendly_suggestion_text_);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void OmniboxViewViews::CandidateWindowOpened(
    ash::input_method::InputMethodManager* manager) {
  ime_candidate_window_open_ = true;
}

void OmniboxViewViews::CandidateWindowClosed(
    ash::input_method::InputMethodManager* manager) {
  ime_candidate_window_open_ = false;
}
#endif

void OmniboxViewViews::ContentsChanged(views::Textfield* sender,
                                       const std::u16string& new_contents) {}

bool OmniboxViewViews::HandleKeyEvent(views::Textfield* textfield,
                                      const ui::KeyEvent& event) {
  PermitExternalProtocolHandler();

  if (event.type() == ui::EventType::kKeyReleased) {
    // The omnibox contents may change while the control key is pressed.
    if (event.key_code() == ui::VKEY_CONTROL)
      model()->OnControlKeyChanged(false);

    return false;
  }

  // Skip processing of [Alt]+<num-pad digit> Unicode alt key codes.
  // Otherwise, if num-lock is off, the events are handled as [Up], [Down], etc.
  if (event.IsUnicodeKeyCode())
    return false;

  // Show a notification if the clipboard is restricted by the rules of the
  // data leak prevention policy. This state is used by the
  // IsTextEditCommandEnabled(ui::TextEditCommand::PASTE) cases below.
  base::AutoReset<bool> show_rejection_ui(&show_rejection_ui_if_any_, true);

  const bool shift = event.IsShiftDown();
  const bool control = event.IsControlDown();
  const bool alt = event.IsAltDown() || event.IsAltGrDown();
  const bool command = event.IsCommandDown();
  switch (event.key_code()) {
    case ui::VKEY_RETURN: {
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB;
      if ((alt && !shift) || (shift && command)) {
        disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      } else if (alt || command) {
        disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
      } else if (shift) {
        disposition = WindowOpenDisposition::NEW_WINDOW;
      }
      // According to unit tests and comments, holding control when pressing
      // enter has special behavior handled by `AcceptInput` so in this case
      // the user is selecting their input (possibly with modification like
      // appending ".com") and not the row match. This is indicated with an
      // explicit `kNoMatch` line selection.
      if (model()->PopupIsOpen() && !control) {
        model()->OpenSelection(model()->GetPopupSelection(), event.time_stamp(),
                               disposition);
      } else {
        model()->OpenSelection(
            OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch),
            event.time_stamp(), disposition);
      }
      return true;
    }
    case ui::VKEY_ESCAPE:
      return model()->OnEscapeKeyPressed();

    case ui::VKEY_CONTROL:
      model()->OnControlKeyChanged(true);
      break;

    case ui::VKEY_DELETE:
      if (shift && model()->PopupIsOpen()) {
        model()->TryDeletingPopupLine(model()->GetPopupSelection().line);
      }
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
      if (control || alt || shift || GetReadOnly())
        return false;
      model()->OnUpOrDownPressed(false, true);
      return true;

    case ui::VKEY_NEXT:
      if (control || alt || shift || GetReadOnly())
        return false;
      model()->OnUpOrDownPressed(true, true);
      return true;

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
          SetSelectedRange(gfx::Range(start, 0));
        } else {
          // After uneliding, move the caret to the beginning of the full
          // unelided URL.
          SetCaretPos(0);
        }

        TextChanged();
        return true;
      }
      break;

    case ui::VKEY_SPACE: {
      if (model()->PopupIsOpen() && !control && !alt && !shift) {
        if (model()->OnSpacePressed()) {
          return true;
        }
        OmniboxPopupSelection selection = model()->GetPopupSelection();
        if (selection.IsButtonFocused()) {
          model()->OpenSelection(selection, event.time_stamp());
          return true;
        }
      }
      break;
    }
    default:
      break;
  }

  if (is_mouse_pressed_ && select_all_on_mouse_release_) {
    // https://crbug.com/1063161 If the user presses the mouse button down and
    // begins to type without releasing the mouse button, the subsequent release
    // will delete any newly typed characters due to the SelectAll happening on
    // mouse-up. If we detect this state, do the select-all immediately.
    SelectAll(true);
    select_all_on_mouse_release_ = false;
  }

  return HandleEarlyTabActions(event);
}

void OmniboxViewViews::OnBeforeUserAction(views::Textfield* sender) {
  OnBeforePossibleChange();
}

void OmniboxViewViews::OnAfterUserAction(views::Textfield* sender) {
  OnAfterPossibleChange(true);
}

void OmniboxViewViews::OnAfterCutOrCopy(ui::ClipboardBuffer clipboard_buffer) {
  ui::Clipboard* cb = ui::Clipboard::GetForCurrentThread();
  std::u16string selected_text;
  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, {.notify_if_restricted = false});
  cb->ReadText(clipboard_buffer, &data_dst, &selected_text);
  GURL url;
  bool write_url = false;
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), &selected_text, &url,
                             &write_url);
  if (IsSelectAll()) {
    UMA_HISTOGRAM_COUNTS_1M(OmniboxEditModel::kCutOrCopyAllTextHistogram, 1);

    if (clipboard_buffer != ui::ClipboardBuffer::kSelection &&
        location_bar_view_) {
      auto* web_contents = location_bar_view_->GetWebContents();
      if (web_contents) {
        if (auto* clusters_helper =
                HistoryClustersTabHelper::FromWebContents(web_contents)) {
          clusters_helper->OnOmniboxUrlCopied();
        }
      }
    }
  }

  ui::ScopedClipboardWriter scoped_clipboard_writer(clipboard_buffer);
  scoped_clipboard_writer.WriteText(selected_text);
  if (!ShouldDoLearning()) {
    // Data is copied from an incognito window, so mark it as off the record.
    scoped_clipboard_writer.MarkAsOffTheRecord();
  }

  // Regardless of |write_url|, don't write a hyperlink to the clipboard.
  // Plaintext URLs are simply handled more consistently than hyperlinks.
}

void OmniboxViewViews::OnWriteDragData(ui::OSExchangeData* data) {
  GURL url;
  bool write_url;
  std::u16string selected_text = GetSelectedText();
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), &selected_text, &url,
                             &write_url);
  data->SetString(selected_text);
  if (write_url) {
    gfx::Image favicon;
    std::u16string title = selected_text;
    if (IsSelectAll())
      model()->GetDataForURLExport(&url, &title, &favicon);
    button_drag_utils::SetURLAndDragImage(url, title, favicon.AsImageSkia(),
                                          nullptr, data);
    data->SetURL(url, title);
  }
}

void OmniboxViewViews::OnGetDragOperationsForTextfield(int* drag_operations) {
  std::u16string selected_text = GetSelectedText();
  GURL url;
  bool write_url;
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), &selected_text, &url,
                             &write_url);
  if (write_url)
    *drag_operations |= ui::DragDropTypes::DRAG_LINK;
}

void OmniboxViewViews::AppendDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = *formats | ui::OSExchangeData::URL;
}

DragOperation OmniboxViewViews::OnDrop(const ui::DropTargetEvent& event) {
  ui::mojom::DragOperation output_drag_op = ui::mojom::DragOperation::kNone;
  PerformDrop(event, output_drag_op, /*drag_image_layer_owner=*/nullptr);
  return output_drag_op;
}

views::View::DropCallback OmniboxViewViews::CreateDropCallback(
    const ui::DropTargetEvent& event) {
  return base::BindOnce(&OmniboxViewViews::PerformDrop,
                        weak_factory_.GetWeakPtr());
}

void OmniboxViewViews::UpdateContextMenu(ui::SimpleMenuModel* menu_contents) {
  MaybeAddSendTabToSelfItem(menu_contents);

  std::optional<size_t> paste_position =
      menu_contents->GetIndexOfCommandId(Textfield::kPaste);
  DCHECK(paste_position.has_value());
  menu_contents->InsertItemWithStringIdAt(paste_position.value() + 1,
                                          IDC_PASTE_AND_GO, IDS_PASTE_AND_GO);

  menu_contents->AddSeparator(ui::NORMAL_SEPARATOR);

  menu_contents->AddItemWithStringId(IDC_EDIT_SEARCH_ENGINES,
                                     IDS_MANAGE_SEARCH_ENGINES_AND_SITE_SEARCH);

  const PrefService::Preference* show_full_urls_pref =
      location_bar_view_->profile()->GetPrefs()->FindPreference(
          omnibox::kPreventUrlElisionsInOmnibox);
  if (!show_full_urls_pref->IsManaged()) {
    menu_contents->AddCheckItemWithStringId(IDC_SHOW_FULL_URLS,
                                            IDS_CONTEXT_MENU_SHOW_FULL_URLS);
  }

  if (lens::features::IsOmniboxEntryPointEnabled() &&
      location_bar_view_->browser()
          ->GetFeatures()
          .lens_overlay_entry_point_controller()
          ->IsEnabled()) {
    menu_contents->AddCheckItemWithStringId(
        IDC_SHOW_GOOGLE_LENS_SHORTCUT,
        IDS_CONTEXT_MENU_SHOW_GOOGLE_LENS_SHORTCUT);
  }
}

bool OmniboxViewViews::IsCommandIdChecked(int id) const {
  if (id == IDC_SHOW_FULL_URLS) {
    return location_bar_view_->profile()->GetPrefs()->GetBoolean(
        omnibox::kPreventUrlElisionsInOmnibox);
  }
  if (id == IDC_SHOW_GOOGLE_LENS_SHORTCUT) {
    return location_bar_view_->profile()->GetPrefs()->GetBoolean(
        omnibox::kShowGoogleLensShortcut);
  }
  return false;
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

void OmniboxViewViews::OnDidPresentCompositorFrame(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {
  if (latency_histogram_state_ == COMPOSITING_STARTED) {
    DCHECK(!insert_char_time_.is_null());
    UMA_HISTOGRAM_TIMES("Omnibox.CharTypedToRepaintLatency",
                        base::TimeTicks::Now() - insert_char_time_);
    insert_char_time_ = base::TimeTicks();
    latency_histogram_state_ = NOT_ACTIVE;
  }
}

void OmniboxViewViews::OnCompositingShuttingDown(ui::Compositor* compositor) {
  scoped_compositor_observation_.Reset();
}

void OmniboxViewViews::OnTemplateURLServiceChanged() {
  InstallPlaceholderText();
}

void OmniboxViewViews::PermitExternalProtocolHandler() {
  ExternalProtocolHandler::PermitLaunchUrl();
}

void OmniboxViewViews::PerformDrop(
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  if (HasTextBeingDragged()) {
    output_drag_op = DragOperation::kNone;
    return;
  }

  const ui::OSExchangeData& data = event.data();
  std::u16string text;
  if (std::optional<ui::OSExchangeData::UrlInfo> url_result =
          data.GetURLAndTitle(ui::FilenameToURLPolicy::CONVERT_FILENAMES);
      url_result.has_value()) {
    text = StripJavascriptSchemas(base::UTF8ToUTF16(url_result->url.spec()));
  } else if (std::optional<std::u16string> text_result = data.GetString();
             text_result.has_value()) {
    text = StripJavascriptSchemas(base::CollapseWhitespace(*text_result, true));
  } else {
    output_drag_op = DragOperation::kNone;
    return;
  }

  SetUserText(text);
  if (!HasFocus())
    RequestFocus();
  SelectAll(false);
  output_drag_op = DragOperation::kCopy;
}

void OmniboxViewViews::MaybeAddSendTabToSelfItem(
    ui::SimpleMenuModel* menu_contents) {
  // Only add this menu entry if SendTabToSelf feature is enabled.
  if (!send_tab_to_self::ShouldDisplayEntryPoint(
          location_bar_view_->GetWebContents())) {
    return;
  }

  size_t index = menu_contents->GetIndexOfCommandId(Textfield::kUndo).value();
  // Add a separator if this is not the first item.
  if (index) {
    menu_contents->InsertSeparatorAt(index++, ui::NORMAL_SEPARATOR);
  }

  menu_contents->InsertItemAt(
      index, IDC_SEND_TAB_TO_SELF,
      l10n_util::GetStringUTF16(IDS_MENU_SEND_TAB_TO_SELF));
#if !BUILDFLAG(IS_MAC)
  menu_contents->SetIcon(index, ui::ImageModel::FromVectorIcon(kDevicesIcon));
#endif
  menu_contents->InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);
}

void OmniboxViewViews::OnPopupOpened() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // It's not great for promos to overlap the omnibox if the user opens the
  // drop-down after showing the promo. This especially causes issues on Mac and
  // Linux due to z-order/rendering issues, see crbug.com/1225046 and
  // crbug.com/332769403 for examples.
  BrowserFeaturePromoController::MaybeCloseOverlappingHelpBubbles(this);
#endif
}

void OmniboxViewViews::UpdatePlaceholderTextColor() {
  // Keyword placeholders are dim to differentiate from user input. DSE
  // placeholders are not dim to draw attention to the omnibox and because the
  // omnibox is unfocused so there's less risk of confusion with user input.
  // Null in tests.
  if (!GetColorProvider())
    return;
  set_placeholder_text_color(GetColorProvider()->GetColor(
      model()->keyword_placeholder().empty() ? kColorOmniboxText
                                             : kColorOmniboxTextDimmed));
}

BEGIN_METADATA(OmniboxViewViews)
ADD_READONLY_PROPERTY_METADATA(bool, SelectionAtEnd)
ADD_READONLY_PROPERTY_METADATA(int, TextWidth)
ADD_READONLY_PROPERTY_METADATA(int, UnelidedTextWidth)
ADD_READONLY_PROPERTY_METADATA(int, Width)
ADD_READONLY_PROPERTY_METADATA(std::u16string, SelectedText)
END_METADATA
