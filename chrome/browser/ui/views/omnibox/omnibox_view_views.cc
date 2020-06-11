// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_desktop_util.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_result_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/buildflags.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "net/base/escape.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_keyboard_controller.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/selection_model.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/browser/browser_process.h"
#endif

#if BUILDFLAG(ENABLE_LEGACY_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker_factory.h"
#endif

using metrics::OmniboxEventProto;

namespace {

// TODO(tommycli): Remove this killswitch once we are confident that the new
// behavior doesn't cause big user breakage.
constexpr base::Feature kOmniboxCanCopyHyperlinksToClipboard{
    "OmniboxCanCopyHyperlinksToClipboard", base::FEATURE_ENABLED_BY_DEFAULT};

// When certain field trials are enabled, the path is hidden this long after
// page load.
const uint32_t kPathFadeOutDelayMs = 4000;

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

enum OmniboxMenuCommands {
  kShowUrl = views::Textfield::MenuCommands::kLastCommandId + 1,
};

bool IsClipboardDataMarkedAsConfidential() {
  return ui::Clipboard::GetForCurrentThread()
      ->IsMarkedByOriginatorAsConfidential();
}

}  // namespace

// Animates the path from |starting_color| to |ending_color|. The fading starts
// after |delay_ms| ms.
class OmniboxViewViews::PathFadeAnimation
    : public views::AnimationDelegateViews {
 public:
  PathFadeAnimation(OmniboxViewViews* view,
                    SkColor starting_color,
                    SkColor ending_color,
                    uint32_t delay_ms)
      : AnimationDelegateViews(view),
        view_(view),
        starting_color_(starting_color),
        ending_color_(ending_color),
        animation_(gfx::MultiAnimation::Parts({
                       gfx::MultiAnimation::Part(
                           base::TimeDelta::FromMilliseconds(delay_ms),
                           gfx::Tween::ZERO),
                       gfx::MultiAnimation::Part(
                           base::TimeDelta::FromMilliseconds(300),
                           gfx::Tween::FAST_OUT_SLOW_IN),
                   }),
                   gfx::MultiAnimation::kDefaultTimerInterval) {
    DCHECK(view_);

    animation_.set_delegate(this);
    animation_.set_continuous(false);
  }

  // Starts the animation over |path_bounds|. The caller is responsible for
  // calling Stop() if the text changes and |path_bounds| is no longer valid.
  void Start(const gfx::Range& path_bounds) {
    path_bounds_ = path_bounds;
    animation_.Start();
  }

  void Stop() { animation_.Stop(); }

  bool IsAnimating() { return animation_.is_animating(); }

  // Stops the animation if currently running and sets the starting color to
  // |starting_color|.
  void ResetStartingColor(SkColor starting_color) {
    Stop();
    starting_color_ = starting_color;
  }

  SkColor GetCurrentColor() {
    return gfx::Tween::ColorValueBetween(animation_.GetCurrentValue(),
                                         starting_color_, ending_color_);
  }

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override {
    DCHECK(!view_->model()->user_input_in_progress());
    view_->ApplyColor(GetCurrentColor(), path_bounds_);
  }

 private:
  // Non-owning pointer. |view_| must always outlive this class.
  OmniboxViewViews* view_;
  SkColor starting_color_;
  SkColor ending_color_;

  // The path text range we are fading.
  gfx::Range path_bounds_;

  gfx::MultiAnimation animation_;
};

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
      location_bar_view_(location_bar),
      latency_histogram_state_(NOT_ACTIVE),
      friendly_suggestion_text_prefix_length_(0) {
  SetID(VIEW_ID_OMNIBOX);
  SetFontList(font_list);

  // Unit tests may use a mock location bar that has no browser,
  // or use no location bar at all.
  if (location_bar_view_ && location_bar_view_->browser()) {
    pref_change_registrar_.Init(
        location_bar_view_->browser()->profile()->GetPrefs());
    pref_change_registrar_.Add(
        omnibox::kPreventUrlElisionsInOmnibox,
        base::BindRepeating(&OmniboxViewViews::Update, base::Unretained(this)));
  }
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
  InstallPlaceholderText();
  scoped_template_url_service_observer_.Add(
      model()->client()->GetTemplateURLService());

  if (popup_window_mode_)
    SetReadOnly(true);

  if (location_bar_view_) {
    // Initialize the popup view using the same font.
    popup_view_.reset(
        new OmniboxPopupContentsView(this, model(), location_bar_view_));
    if (OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction())
      Observe(location_bar_view_->GetWebContents());
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

  // When the tab is changed, reshow the path in case it had previously been
  // hidden by a user interaction (when certain field trials are enabled).
  ResetToHideOnInteraction();
  if (OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction()) {
    Observe(web_contents);
  }
}

void OmniboxViewViews::ResetTabState(content::WebContents* web_contents) {
  web_contents->SetUserData(OmniboxState::kKey, nullptr);
}

void OmniboxViewViews::InstallPlaceholderText() {
  const TemplateURL* const default_provider =
      model()->client()->GetTemplateURLService()->GetDefaultSearchProvider();
  if (default_provider) {
    SetPlaceholderText(l10n_util::GetStringFUTF16(
        IDS_OMNIBOX_PLACEHOLDER_TEXT, default_provider->short_name()));
  } else {
    SetPlaceholderText(base::string16());
  }
}

bool OmniboxViewViews::SelectionAtBeginning() const {
  const gfx::Range sel = GetSelectedRange();
  return sel.GetMax() == 0;
}

bool OmniboxViewViews::SelectionAtEnd() const {
  const gfx::Range sel = GetSelectedRange();
  return sel.GetMin() == GetText().size();
}

void OmniboxViewViews::EmphasizeURLComponents() {
  if (!location_bar_view_)
    return;

  // Cancel any existing path fading animation. The path style will be reset
  // in the following lines, so there should be no ill effects from cancelling
  // the animation midway.
  if (path_fade_out_animation_)
    path_fade_out_animation_->Stop();
  if (path_fade_in_animation_)
    path_fade_in_animation_->Stop();
  if (path_fade_out_fast_animation_)
    path_fade_out_fast_animation_->Stop();

  // If the current contents is a URL, turn on special URL rendering mode in
  // RenderText.
  bool text_is_url = model()->CurrentTextIsURL();
  GetRenderText()->SetDirectionalityMode(
      text_is_url ? gfx::DIRECTIONALITY_AS_URL : gfx::DIRECTIONALITY_FROM_TEXT);
  SetStyle(gfx::TEXT_STYLE_STRIKE, false);

  base::string16 text = GetText();
  UpdateTextStyle(text, text_is_url, model()->client()->GetSchemeClassifier());

  // Only fade the path when everything but the host is de-emphasized.
  if (path_fade_out_animation_ && CanFadePath()) {
    // Whenever the text changes, EmphasizeURLComponents is called again, and
    // the animation is reset with a new |path_bounds|.
    path_fade_out_animation_->Start(GetPathBounds());
  }

  if (OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover() ||
      !OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction()) {
    // If reveal-on-hover is enabled and hide-on-interaction is disabled, hide
    // the path now.
    if (CanFadePath())
      ApplyColor(SK_ColorTRANSPARENT, GetPathBounds());
  }
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

base::string16 OmniboxViewViews::GetText() const {
  // TODO(oshima): IME support
  return Textfield::GetText();
}

void OmniboxViewViews::SetUserText(const base::string16& text,
                                   bool update_popup) {
  saved_selection_for_focus_change_.clear();
  OmniboxView::SetUserText(text, update_popup);
}

void OmniboxViewViews::EnterKeywordModeForDefaultSearchProvider() {
  // Transition the user into keyword mode using their default search provider.
  model()->EnterKeywordModeForDefaultSearchProvider(
      OmniboxEventProto::KEYBOARD_SHORTCUT);
}

void OmniboxViewViews::GetSelectionBounds(
    base::string16::size_type* start,
    base::string16::size_type* end) const {
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
    focus_reveal_lock.reset(
        BrowserView::GetBrowserViewForBrowser(location_bar_view_->browser())
            ->immersive_mode_controller()
            ->GetRevealedLock(ImmersiveModeController::ANIMATE_REVEAL_YES));
  }

  RequestFocus();

  // |is_user_initiated| is true for focus events from keyboard accelerators.
  if (is_user_initiated)
    model()->ShowOnFocusSuggestionsIfAutocompleteIdle();

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
    GetWidget()->GetCompositor()->RequestPresentationTimeForNextFrame(
        base::BindOnce(
            [](base::TimeTicks insert_timestamp,
               base::TimeTicks paint_timestamp,
               const gfx::PresentationFeedback& feedback) {
              if (feedback.flags & gfx::PresentationFeedback::kFailure)
                return;
              UMA_HISTOGRAM_TIMES(
                  "Omnibox.CharTypedToRepaintLatency.PaintToPresent",
                  feedback.timestamp - paint_timestamp);
              UMA_HISTOGRAM_TIMES(
                  "Omnibox.CharTypedToRepaintLatency.InsertToPresent",
                  feedback.timestamp - insert_timestamp);
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
      model()->PasteAndGo(GetClipboardText());
      return;
    case kShowUrl:
      model()->Unelide(true /* exit_query_in_omnibox */);
      return;
    case IDC_SHOW_FULL_URLS:
    case IDC_EDIT_SEARCH_ENGINES:
      location_bar_view_->command_updater()->ExecuteCommand(command_id);
      return;

    case IDC_SEND_TAB_TO_SELF_SINGLE_TARGET:
      send_tab_to_self::ShareToSingleTarget(
          location_bar_view_->GetWebContents());
      send_tab_to_self::RecordSendTabToSelfClickResult(
          send_tab_to_self::kOmniboxMenu, SendTabToSelfClickResult::kClickItem);
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

void OmniboxViewViews::OnThemeChanged() {
  views::Textfield::OnThemeChanged();

  const SkColor dimmed_text_color = GetOmniboxColor(
      GetThemeProvider(), OmniboxPart::LOCATION_BAR_TEXT_DIMMED);
  set_placeholder_text_color(dimmed_text_color);

  if (OmniboxFieldTrial::IsHidePathQueryRefEnabled()) {
    // The animation only applies when the path is dimmed to begin with.

    // In on-hover and on-interaction variations, the path fades in or out based
    // on user interactions, not automatically after a timeout.
    if (OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction()) {
      // When hiding the path on interaction, don't create the fade-in animation
      // yet. The hover fade-in animation (if enabled) will be created later in
      // DidGetUserInteraction() after the path is faded out.
      path_fade_out_fast_animation_ = std::make_unique<PathFadeAnimation>(
          this, dimmed_text_color, SK_ColorTRANSPARENT, 0);
    } else if (OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover()) {
      // When reveal-on-hover is enabled but not hide-on-interaction, create
      // both the fade-in and fade-out animations now.
      path_fade_in_animation_ = std::make_unique<PathFadeAnimation>(
          this, SK_ColorTRANSPARENT, dimmed_text_color,
          OmniboxFieldTrial::RevealPathQueryRefOnHoverThresholdMs());
      path_fade_out_fast_animation_ = std::make_unique<PathFadeAnimation>(
          this, dimmed_text_color, SK_ColorTRANSPARENT, 0);
    } else {
      // When neither reveal-on-hover nor hide-on-interaction are enabled, fade
      // out the path after a fixed delay.
      path_fade_out_animation_ = std::make_unique<PathFadeAnimation>(
          this, dimmed_text_color, SK_ColorTRANSPARENT, kPathFadeOutDelayMs);
    }
  }

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

void OmniboxViewViews::SetTextAndSelectedRanges(
    const base::string16& text,
    const std::vector<gfx::Range>& ranges,
    const base::string16& additional_text) {
  // Will try to fit as much of unselected text as possible. If possible,
  // guarantees at least |pad_left| chars of the unselected text are visible. If
  // possible given the prior guarantee, also guarantees |pad_right| chars of
  // the selected text are visible.
  static const uint32_t kPadRight = 30;
  static const uint32_t kPadLeft = 10;

  SetText(text, ranges[0].end());
  // Select all the text to prioritize showing unselected text.
  SetSelectedRange(gfx::Range(ranges[0].start(), 0));
  // Scroll range right, to ensure |kPadRight| chars of selected text are
  // shown.
  SetSelectedRange(
      gfx::Range(ranges[0].start(),
                 std::min(ranges[0].end() + kPadRight, ranges[0].start())));
  // Scroll range left, to ensure |kPadLeft| chars of unselected text are
  // shown.
  SetSelectedRange(
      gfx::Range(ranges[0].start(),
                 ranges[0].end() - std::min(kPadLeft, ranges[0].end())));
  // Select the specified ranges.
  SetSelectedRanges(ranges);
  // Set the additional text.
  // TODO (manukh): Ideally, OmniboxView wouldn't be responsible for its sibling
  // label owned by LocationBarView. However, this is the only practical pathway
  // between the OmniboxEditModel, which handles setting the omnibox match, and
  // LocationBarView. Perhaps, if we decide to launch rich autocompletion we'll
  // consider alternatives.
  location_bar_view_->SetOmniboxAdditionalText(additional_text);
}

void OmniboxViewViews::SetSelectedRanges(
    const std::vector<gfx::Range>& ranges) {
  // Even when no text is selected, |ranges| should have at least 1 (empty)
  // Range representing the cursor.
  DCHECK(!ranges.empty());

  SetSelectedRange(ranges[0]);
  for (size_t i = 1; i < ranges.size(); i++)
    SetSelectedRange(ranges[i], false);
}

base::string16 OmniboxViewViews::GetSelectedText() const {
  // TODO(oshima): Support IME.
  return views::Textfield::GetSelectedText();
}

void OmniboxViewViews::OnOmniboxPaste() {
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

  if (!model()->popup_model()->IsOpen())
    return false;

  model()->popup_model()->StepSelection(event.IsShiftDown()
                                            ? OmniboxPopupModel::kBackward
                                            : OmniboxPopupModel::kForward,
                                        OmniboxPopupModel::kStateOrLine);

  return true;
}

// The following 2 methods implement the following table, which attempts to
// handle left and right arrow keys versus LTR/RTL text and UI (which can be
// different) as expected.
//
// LTR UI, LTR text, right arrow, at end (rightmost) - focuses
// LTR UI, LTR text, left arrow, (regardless) - unfocuses
// LTR UI, RTL text, right arrow, at beginning (rightmost) - focuses
// LTR UI, RTL text, left arrow, (regardless) - unfocuses
//
// RTL UI, RTL text, left arrow, at end (leftmost) - focuses
// RTL UI, RTL text, right arrow, (regardless) - unfocuses
// RTL UI, LTR text, left arrow, at beginning (leftmost) - focuses
// RTL UI, LTR text, right arrow, (regardless) - unfocuses

bool OmniboxViewViews::TextAndUIDirectionMatch() const {
  // If text and UI direction are RTL, or both aren't.
  return (GetRenderText()->GetDisplayTextDirection() ==
          base::i18n::RIGHT_TO_LEFT) == base::i18n::IsRTL();
}

bool OmniboxViewViews::DirectionAwareSelectionAtEnd() const {
  // When text and UI direction match, 'end' is as expected,
  // otherwise we use beginning.
  return TextAndUIDirectionMatch() ? SelectionAtEnd() : SelectionAtBeginning();
}

#if defined(OS_MACOSX)
void OmniboxViewViews::AnnounceFriendlySuggestionText() {
  GetViewAccessibility().AnnounceText(friendly_suggestion_text_);
}
#endif

bool OmniboxViewViews::MaybeTriggerSecondaryButton(const ui::KeyEvent& event) {
  // TODO(tommycli): If we have a WebUI omnibox popup, we should move the
  // secondary button logic out of the View and into the OmniboxPopupModel.
  if (base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopup))
    return false;

  if (model()->popup_model()->selected_line_state() !=
      OmniboxPopupModel::BUTTON_FOCUSED)
    return false;

  OmniboxPopupModel* popup_model = model()->popup_model();
  if (!popup_model)
    return false;

  size_t selected_line = popup_model->selected_line();
  if (selected_line == OmniboxPopupModel::kNoMatch)
    return false;

  // TODO(tommycli): https://crbug.com/1063071
  // Diving into |popup_view_| was a mistake. Here's a hotfix to stop the crash,
  // but the ultimate fix should be to move this logic into OmniboxPopupModel.
  if (!popup_view_ || popup_view_->result_view_at(selected_line) == nullptr)
    return false;

  return popup_view_->result_view_at(selected_line)
      ->MaybeTriggerSecondaryButton(event);
}

void OmniboxViewViews::SetWindowTextAndCaretPos(
    const base::string16& text,
    size_t caret_pos,
    bool update_popup,
    bool notify_text_changed,
    const base::string16& additional_text) {
  const gfx::Range range(caret_pos);
  SetTextAndSelectedRanges(text, {range}, additional_text);

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
    saved_temporary_selection_ = GetRenderText()->GetAllSelections();
  SetAccessibilityLabel(display_text, match);
  SetWindowTextAndCaretPos(display_text, display_text.length(), false,
                           notify_text_changed);
}

void OmniboxViewViews::OnInlineAutocompleteTextMaybeChanged(
    const base::string16& display_text,
    size_t user_text_length,
    size_t user_text_start,
    const base::string16& additional_text) {
  if (display_text == GetText())
    return;

  if (!IsIMEComposing()) {
    std::vector<gfx::Range> ranges = {
        {display_text.size(), user_text_length + user_text_start}};
    if (user_text_start)
      ranges.push_back({0, user_text_start});
    SetTextAndSelectedRanges(display_text, ranges, additional_text);
  } else if (location_bar_view_) {
    location_bar_view_->SetImeInlineAutocompletion(
        display_text.substr(user_text_length));
  }

  EmphasizeURLComponents();
}

void OmniboxViewViews::OnInlineAutocompleteTextCleared() {
  // Hide the inline autocompletion for IME users.
  if (location_bar_view_)
    location_bar_view_->SetImeInlineAutocompletion(base::string16());
}

void OmniboxViewViews::OnRevertTemporaryText(const base::string16& display_text,
                                             const AutocompleteMatch& match) {
  SetAccessibilityLabel(display_text, match);
  SetSelectedRanges(saved_temporary_selection_);

  // We got here because the user hit the Escape key. We explicitly don't call
  // TextChanged(), since OmniboxPopupModel::ResetToDefaultMatch() has already
  // been called by now, and it would've called TextChanged() if it was
  // warranted.
  // However, it's important to notify accessibility that the value has changed,
  // otherwise the screen reader will use the old accessibility label text.
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
}

void OmniboxViewViews::ClearAccessibilityLabel() {
  if (friendly_suggestion_text_.empty())
    return;
  friendly_suggestion_text_.clear();
  friendly_suggestion_text_prefix_length_ = 0;
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
}

void OmniboxViewViews::SetAccessibilityLabel(const base::string16& display_text,
                                             const AutocompleteMatch& match) {
  if (model()->popup_model()->selected_line() == OmniboxPopupModel::kNoMatch) {
    // If nothing is selected in the popup, we are in the no-default-match edge
    // case, and |match| is a synthetically generated match. In that case,
    // bypass OmniboxPopupModel and get the label from our synthetic |match|.
    friendly_suggestion_text_ = AutocompleteMatchType::ToAccessibilityLabel(
        match, display_text, OmniboxPopupModel::kNoMatch,
        model()->result().size(), 0, &friendly_suggestion_text_prefix_length_);
  } else {
    friendly_suggestion_text_ =
        model()->popup_model()->GetAccessibilityLabelForCurrentSelection(
            display_text, &friendly_suggestion_text_prefix_length_);
  }

#if defined(OS_MACOSX)
  // On macOS, the only way to get VoiceOver to speak the friendly suggestion
  // text (for example, "how to open a pdf, search suggestion, 4 of 8") is
  // with an explicit announcement. Use PostTask to ensure that this
  // announcement happens after the text change notification, otherwise
  // the text change can interrupt the announcement.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
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
  base::string16 original_text = GetText();
  base::string16 original_selected_text = GetSelectedText();
  if (!model()->Unelide(false /* exit_query_in_omnibox */))
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
  if (offset == base::string16::npos && !original_text.empty() &&
      original_text.back() == base::char16('/')) {
    offset = GetText().find(original_text.substr(0, original_text.size() - 1));
  }

  if (offset != base::string16::npos) {
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
  return static_cast<int>(GetText().length());
}

void OmniboxViewViews::SetEmphasis(bool emphasize, const gfx::Range& range) {
  SkColor color = GetOmniboxColor(
      GetThemeProvider(), emphasize ? OmniboxPart::LOCATION_BAR_TEXT_DEFAULT
                                    : OmniboxPart::LOCATION_BAR_TEXT_DIMMED);
  if (range.IsValid())
    ApplyColor(color, range);
  else
    SetColor(color);
}

void OmniboxViewViews::UpdateSchemeStyle(const gfx::Range& range) {
  DCHECK(range.IsValid());
  DCHECK(!model()->user_input_in_progress());

  // Do not style the scheme for non-http/https URLs. For such schemes, styling
  // could be confusing or misleading. For example, the scheme isn't meaningful
  // in about:blank URLs. Or in blob: or filesystem: URLs, which have an inner
  // origin, the URL is likely too syntax-y to be able to meaningfully draw
  // attention to any part of it.
  if (!controller()->GetLocationBarModel()->GetURL().SchemeIsHTTPOrHTTPS())
    return;

  security_state::SecurityLevel security_level =
      controller()->GetLocationBarModel()->GetSecurityLevel();

  // Only SECURE and DANGEROUS levels (pages served over HTTPS or flagged by
  // SafeBrowsing) get a special scheme color treatment. If the security level
  // is NONE or WARNING, we do not override the text style
  // previously applied to the scheme text range by SetEmphasis().
  if (security_level == security_state::NONE ||
      security_level == security_state::WARNING)
    return;
  ApplyColor(location_bar_view_->GetSecurityChipColor(security_level), range);
  if (security_level == security_state::DANGEROUS)
    ApplyStyle(gfx::TEXT_STYLE_STRIKE, true, range);
}

void OmniboxViewViews::OnMouseMoved(const ui::MouseEvent& event) {
  if (location_bar_view_)
    location_bar_view_->OnOmniboxHovered(true);

  if (!OmniboxFieldTrial::IsHidePathQueryRefEnabled() ||
      !OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover()) {
    return;
  }
  if (!CanFadePath())
    return;
  path_fade_out_fast_animation_->Stop();
  if (path_fade_in_animation_ && !path_fade_in_animation_->IsAnimating())
    path_fade_in_animation_->Start(GetPathBounds());
}

void OmniboxViewViews::OnMouseExited(const ui::MouseEvent& event) {
  if (location_bar_view_)
    location_bar_view_->OnOmniboxHovered(false);

  if (!OmniboxFieldTrial::IsHidePathQueryRefEnabled() ||
      !OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover()) {
    return;
  }
  if (!CanFadePath())
    return;
  // When hide-on-interaction is enabled, we don't want to fade the path in or
  // out until there's user interaction with the page. In this variation,
  // |path_fade_in_animation_| is created in DidGetUserInteraction() so its
  // existence signals that user interaction has taken place already.
  if (path_fade_in_animation_) {
    path_fade_out_fast_animation_->ResetStartingColor(
        path_fade_in_animation_->GetCurrentColor());
    path_fade_in_animation_->Stop();
    path_fade_out_fast_animation_->Start(GetPathBounds());
  }
}

bool OmniboxViewViews::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_PASTE_AND_GO;
}

base::string16 OmniboxViewViews::GetLabelForCommandId(int command_id) const {
  DCHECK_EQ(IDC_PASTE_AND_GO, command_id);

  // Don't paste-and-go data that was marked by its originator as confidential.
  constexpr size_t kMaxSelectionTextLength = 50;
  const base::string16 clipboard_text = IsClipboardDataMarkedAsConfidential()
                                            ? base::string16()
                                            : GetClipboardText();

  if (clipboard_text.empty())
    return l10n_util::GetStringUTF16(IDS_PASTE_AND_GO_EMPTY);

  base::string16 selection_text = gfx::TruncateString(
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
  base::string16 url = url_formatter::ElideUrl(
      match.destination_url, Textfield::GetFontList(), kMaxSelectionPixelWidth);

  return l10n_util::GetStringFUTF16(IDS_PASTE_AND_GO, url);
}

const char* OmniboxViewViews::GetClassName() const {
  return kViewClassName;
}

bool OmniboxViewViews::OnMousePressed(const ui::MouseEvent& event) {
  if (model()->popup_model()) {  // Can be null in tests.
    model()->popup_model()->SetSelectedLineState(OmniboxPopupModel::NORMAL);
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
    model()->ShowOnFocusSuggestionsIfAutocompleteIdle();

  bool handled = views::Textfield::OnMousePressed(event);

  // This ensures that when the user makes a double-click partial select, we
  // perform the unelision at the same time as we make the partial selection,
  // which is on mousedown.
  if (!select_all_on_mouse_release_ &&
      UnapplySteadyStateElisions(UnelisionGesture::OTHER)) {
    TextChanged();
    filter_drag_events_for_unelision_ = true;
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
  static const bool kTakeFocusOnTapUp =
      base::FeatureList::IsEnabled(views::features::kTextfieldFocusOnTapUp);

  const bool gesture_should_take_focus =
      !HasFocus() &&
      event->type() ==
          (kTakeFocusOnTapUp ? ui::ET_GESTURE_TAP : ui::ET_GESTURE_TAP_DOWN);
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
    model()->ShowOnFocusSuggestionsIfAutocompleteIdle();

  views::Textfield::OnGestureEvent(event);

  if (select_all_on_gesture_tap_ && event->type() == ui::ET_GESTURE_TAP) {
    // Select all in the reverse direction so as not to scroll the caret
    // into view and shift the contents jarringly.
    SelectAll(true);
  }

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
  if (model()->popup_model()->IsOpen()) {
    int32_t popup_view_id =
        popup_view_->GetViewAccessibility().GetUniqueId().Get();
    node_data->AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                                   {popup_view_id});
  }

  base::string16::size_type entry_start;
  base::string16::size_type entry_end;
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

  if (popup_window_mode_) {
    node_data->SetRestriction(ax::mojom::Restriction::kReadOnly);
  } else {
    node_data->AddState(ax::mojom::State::kEditable);
  }
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
  // Don't call controller()->OnSetFocus, this view has already acquired focus.

  // Restore the selection we saved in OnBlur() if it's still valid.
  if (!saved_selection_for_focus_change_.empty()) {
    SetSelectedRanges(saved_selection_for_focus_change_);
    saved_selection_for_focus_change_.clear();
  }

  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);

  // Focus changes can affect the visibility of any keyword hint.
  if (location_bar_view_ && model()->is_keyword_hint())
    location_bar_view_->Layout();

#if BUILDFLAG(ENABLE_LEGACY_DESKTOP_IN_PRODUCT_HELP)
  // The user must be starting a session in the same tab as a previous one in
  // order to display the new tab in-product help promo.  While focusing the
  // omnibox is not always a precursor to starting a new session, we don't
  // want to wait until the user is in the middle of editing or navigating,
  // because we'd like to show them the promo at the time when it would be
  // immediately useful.
  if (location_bar_view_ &&
      controller()->GetLocationBarModel()->ShouldDisplayURL()) {
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
  saved_selection_for_focus_change_ = GetRenderText()->GetAllSelections();

  // popup_model() can be null in tests.
  OmniboxPopupModel* popup_model = model()->popup_model();

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
  if (!model()->user_input_in_progress() && popup_model &&
      popup_model->IsOpen() &&
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

  // In cases where there's a lot of whitespace in the text being shown, we want
  // the elision marker to be at the right of the text field, so don't elide
  // whitespace to the left of the elision point.
  render_text->SetWhitespaceElision(false);
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
  if (command_id == Textfield::kPaste)
    return !GetReadOnly() && !GetClipboardText().empty();
  if (command_id == IDC_PASTE_AND_GO) {
    return !GetReadOnly() && !IsClipboardDataMarkedAsConfidential() &&
           model()->CanPasteAndGo(GetClipboardText());
  }

  // Menu item is only shown when it is valid.
  if (command_id == kShowUrl)
    return true;
  if (command_id == IDC_SHOW_FULL_URLS)
    return true;

  return Textfield::IsCommandIdEnabled(command_id) ||
         location_bar_view_->command_updater()->IsCommandEnabled(command_id);
}

void OmniboxViewViews::DidFinishNavigation(
    content::NavigationHandle* navigation) {
  if (navigation->IsSameDocument())
    return;
  if (OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction()) {
    // Once a navigation finishes, show the path and reset state so that it'll
    // be hidden on interaction.
    ResetToHideOnInteraction();
  }
}

void OmniboxViewViews::DidGetUserInteraction(
    const blink::WebInputEvent::Type type) {
  if (!OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction())
    return;

  DCHECK(path_fade_out_fast_animation_);
  path_fade_out_fast_animation_->Stop();
  if (CanFadePath())
    path_fade_out_fast_animation_->Start(GetPathBounds());
  // Now that the path is fading out, create the animation to bring it back on
  // hover (if enabled via field trial).
  if (OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover()) {
    path_fade_in_animation_ = std::make_unique<PathFadeAnimation>(
        this, SK_ColorTRANSPARENT,
        GetOmniboxColor(GetThemeProvider(),
                        OmniboxPart::LOCATION_BAR_TEXT_DIMMED),
        OmniboxFieldTrial::RevealPathQueryRefOnHoverThresholdMs());
  }
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
      return !GetReadOnly();
    case ui::TextEditCommand::PASTE:
      return !GetReadOnly() && !GetClipboardText().empty();
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
      OnOmniboxPaste();
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
    case ui::VKEY_RETURN: {
      OmniboxPopupModel* popup_model = model()->popup_model();
      if (popup_model &&
          popup_model->TriggerSelectionAction(popup_model->selection())) {
        return true;
      } else if (MaybeTriggerSecondaryButton(event)) {
        return true;
      } else if ((alt && !shift) || (shift && command)) {
        model()->AcceptInput(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             event.time_stamp());
      } else if (alt || command) {
        model()->AcceptInput(WindowOpenDisposition::NEW_BACKGROUND_TAB,
                             event.time_stamp());
      } else if (shift) {
        model()->AcceptInput(WindowOpenDisposition::NEW_WINDOW,
                             event.time_stamp());
      } else {
        if (model()->popup_model()->SelectedLineIsTabSwitchSuggestion()) {
          model()->AcceptInput(WindowOpenDisposition::SWITCH_TO_TAB,
                               event.time_stamp());
        } else {
          model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB,
                               event.time_stamp());
        }
      }
      return true;
    }
    case ui::VKEY_ESCAPE:
      return model()->OnEscapeKeyPressed();

    case ui::VKEY_CONTROL:
      model()->OnControlKeyChanged(true);
      break;

    case ui::VKEY_DELETE:
      if (shift && model()->popup_model()->IsOpen()) {
        model()->popup_model()->TryDeletingLine(
            model()->popup_model()->selected_line());
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
      if (!model()->MaybeStartQueryForPopup()) {
        model()->popup_model()->StepSelection(OmniboxPopupModel::kBackward,
                                              OmniboxPopupModel::kAllLines);
      }
      return true;

    case ui::VKEY_NEXT:
      if (control || alt || shift || GetReadOnly())
        return false;
      if (!model()->MaybeStartQueryForPopup()) {
        model()->popup_model()->StepSelection(OmniboxPopupModel::kForward,
                                              OmniboxPopupModel::kAllLines);
      }
      return true;

    case ui::VKEY_RIGHT:
    case ui::VKEY_LEFT: {
      if (control || alt || shift)
        return false;

      const auto step = [=](auto direction) {
        if (!model()->popup_model()) {
          return false;
        }
        auto old_selection = model()->popup_model()->selection();
        return model()->popup_model()->StepSelection(
                   direction, OmniboxPopupModel::kStateOrNothing) !=
               old_selection;
      };

      // If advancing cursor (accounting for UI direction)
      if (base::i18n::IsRTL() == (event.key_code() == ui::VKEY_LEFT)) {
        if (!DirectionAwareSelectionAtEnd())
          return false;

        if (step(OmniboxPopupModel::kForward)) {
          return true;
        }
      } else if (step(OmniboxPopupModel::kBackward)) {
        return true;
      }
      break;
    }
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
      if (!control && !alt && !shift && SelectionAtEnd()) {
        OmniboxPopupModel* popup_model = model()->popup_model();
        if (popup_model &&
            popup_model->TriggerSelectionAction(popup_model->selection())) {
          return true;
        }

        if (MaybeTriggerSecondaryButton(event))
          return true;
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
  base::string16 selected_text;
  cb->ReadText(clipboard_buffer, &selected_text);
  GURL url;
  bool write_url = false;
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), &selected_text, &url,
                             &write_url);
  if (IsSelectAll())
    UMA_HISTOGRAM_COUNTS_1M(OmniboxEditModel::kCutOrCopyAllTextHistogram, 1);

  ui::ScopedClipboardWriter scoped_clipboard_writer(clipboard_buffer);
  scoped_clipboard_writer.WriteText(selected_text);

  if (write_url &&
      base::FeatureList::IsEnabled(kOmniboxCanCopyHyperlinksToClipboard)) {
    scoped_clipboard_writer.WriteHyperlink(selected_text, url.spec());
  }
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
    std::set<ui::ClipboardFormatType>* format_types) {
  *formats = *formats | ui::OSExchangeData::URL;
}

int OmniboxViewViews::OnDrop(const ui::OSExchangeData& data) {
  if (HasTextBeingDragged())
    return ui::DragDropTypes::DRAG_NONE;

  base::string16 text;
  if (data.HasURL(ui::FilenameToURLPolicy::CONVERT_FILENAMES)) {
    GURL url;
    base::string16 title;
    if (data.GetURLAndTitle(ui::FilenameToURLPolicy::CONVERT_FILENAMES, &url,
                            &title)) {
      text = StripJavascriptSchemas(base::UTF8ToUTF16(url.spec()));
    }
  } else if (data.HasString() && data.GetString(&text)) {
    text = StripJavascriptSchemas(base::CollapseWhitespace(text, true));
  } else {
    return ui::DragDropTypes::DRAG_NONE;
  }

  SetUserText(text);
  if (!HasFocus())
    RequestFocus();
  SelectAll(false);
  return ui::DragDropTypes::DRAG_COPY;
}

void OmniboxViewViews::UpdateContextMenu(ui::SimpleMenuModel* menu_contents) {
  // Only add this menu entry if SendTabToSelf feature is enabled.
  if (send_tab_to_self::ShouldOfferFeature(
          location_bar_view_->GetWebContents())) {
    int index = menu_contents->GetIndexOfCommandId(Textfield::kUndo);
    // Add a separator if this is not the first item.
    if (index)
      menu_contents->InsertSeparatorAt(index++, ui::NORMAL_SEPARATOR);

    if (send_tab_to_self::GetValidDeviceCount(location_bar_view_->profile()) ==
        1) {
      menu_contents->InsertItemAt(
          index, IDC_SEND_TAB_TO_SELF_SINGLE_TARGET,
          l10n_util::GetStringFUTF16(
              IDS_CONTEXT_MENU_SEND_TAB_TO_SELF_SINGLE_TARGET,
              send_tab_to_self::GetSingleTargetDeviceName(
                  location_bar_view_->profile())));
    } else {
      send_tab_to_self_sub_menu_model_ =
          std::make_unique<send_tab_to_self::SendTabToSelfSubMenuModel>(
              location_bar_view_->GetWebContents(),
              send_tab_to_self::SendTabToSelfMenuType::kOmnibox);
      menu_contents->InsertSubMenuWithStringIdAt(
          index, IDC_SEND_TAB_TO_SELF, IDS_CONTEXT_MENU_SEND_TAB_TO_SELF,
          send_tab_to_self_sub_menu_model_.get());
    }
#if !defined(OS_MACOSX)
    menu_contents->SetIcon(index,
                           ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
#endif
    menu_contents->InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);
  }

  int paste_position = menu_contents->GetIndexOfCommandId(Textfield::kPaste);
  DCHECK_GE(paste_position, 0);
  menu_contents->InsertItemWithStringIdAt(paste_position + 1, IDC_PASTE_AND_GO,
                                          IDS_PASTE_AND_GO);

  // Only add this menu entry if Query in Omnibox feature is enabled and the
  // feature providing an "Always Show Full URLs" option is disabled.
  if (base::FeatureList::IsEnabled(omnibox::kQueryInOmnibox) &&
      !base::FeatureList::IsEnabled(omnibox::kOmniboxContextMenuShowFullUrls)) {
    // If the user has not started editing the text, and we are not showing the
    // full URL, then provide a way to unelide via the context menu.
    if (!GetReadOnly() && !model()->user_input_in_progress() &&
        GetText() !=
            controller()->GetLocationBarModel()->GetFormattedFullURL()) {
      menu_contents->AddItemWithStringId(kShowUrl, IDS_SHOW_URL);
    }
  }

  menu_contents->AddSeparator(ui::NORMAL_SEPARATOR);

  menu_contents->AddItemWithStringId(IDC_EDIT_SEARCH_ENGINES,
                                     IDS_EDIT_SEARCH_ENGINES);

  if (base::FeatureList::IsEnabled(omnibox::kOmniboxContextMenuShowFullUrls)) {
    menu_contents->AddCheckItemWithStringId(IDC_SHOW_FULL_URLS,
                                            IDS_CONTEXT_MENU_SHOW_FULL_URLS);
  }
}

bool OmniboxViewViews::IsCommandIdChecked(int id) const {
  if (id == IDC_SHOW_FULL_URLS) {
    return location_bar_view_->profile()->GetPrefs()->GetBoolean(
        omnibox::kPreventUrlElisionsInOmnibox);
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

void OmniboxViewViews::OnCompositingEnded(ui::Compositor* compositor) {
  if (latency_histogram_state_ == COMPOSITING_STARTED) {
    DCHECK(!insert_char_time_.is_null());
    UMA_HISTOGRAM_TIMES("Omnibox.CharTypedToRepaintLatency",
                        base::TimeTicks::Now() - insert_char_time_);
    insert_char_time_ = base::TimeTicks();
    latency_histogram_state_ = NOT_ACTIVE;
  }
}

void OmniboxViewViews::OnCompositingShuttingDown(ui::Compositor* compositor) {
  scoped_compositor_observer_.RemoveAll();
}

void OmniboxViewViews::OnTemplateURLServiceChanged() {
  InstallPlaceholderText();
}

gfx::Range OmniboxViewViews::GetPathBounds() {
  url::Component scheme, host;
  base::string16 text = GetText();
  AutocompleteInput::ParseForEmphasizeComponents(
      text, model()->client()->GetSchemeClassifier(), &scheme, &host);
  return gfx::Range(host.end(), text.size());
}

bool OmniboxViewViews::CanFadePath() {
  if (HasFocus() || model()->user_input_in_progress())
    return false;
  if (!model()->CurrentTextIsURL())
    return false;
  base::string16 text = GetText();
  url::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(
      text, model()->client()->GetSchemeClassifier(), &scheme, &host);

  const base::string16 url_scheme = text.substr(scheme.begin, scheme.len);
  return url_scheme != base::UTF8ToUTF16(extensions::kExtensionScheme) &&
         url_scheme != base::UTF8ToUTF16(url::kDataScheme) &&
         host.is_nonempty();
}

void OmniboxViewViews::ResetToHideOnInteraction() {
  if (!OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction())
    return;
  // Delete the fade-in animation; it'll get recreated in
  // DidGetUserInteraction() if reveal-on-hover is enabled. We don't want to
  // fade in the path while it's already showing.
  path_fade_in_animation_.reset();
  if (CanFadePath()) {
    ApplyColor(GetOmniboxColor(GetThemeProvider(),
                               OmniboxPart::LOCATION_BAR_TEXT_DIMMED),
               GetPathBounds());
  }
}
