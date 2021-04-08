// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
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
#include "chrome/browser/reputation/url_elision_policy.h"
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
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
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
#include "net/base/escape.h"
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
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
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
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/browser/browser_process.h"
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

// Draws a rectangle of dimensions and position |rect| in |canvas|, colored
// with a gradient from |start_color| to |end_color|.
void DrawGradientRect(const gfx::Rect& rect,
                      SkColor start_color,
                      SkColor end_color,
                      gfx::Canvas* canvas) {
  SkColor colors[2] = {start_color, end_color};
  SkPoint points[2];
  points[0].iset(rect.origin().x(), rect.origin().y());
  points[1].iset(rect.right(), rect.y());
  cc::PaintFlags flags;
  flags.setShader(cc::PaintShader::MakeLinearGradient(points, colors, nullptr,
                                                      2, SkTileMode::kClamp));
  canvas->DrawRect(rect, flags);
}

// Returns true if  the substring indicated by |range| overflows
// |omnibox_view|'s current local bounds.
bool TextRangeOverflowsView(OmniboxViewViews* omnibox_view,
                            gfx::RenderText* render_text,
                            const gfx::Range& range) {
  // The RenderText must be in NO_ELIDE mode to attempt to retrieve the bounds
  // of |range| (which could be outside its display area).
  DCHECK_EQ(gfx::NO_ELIDE, render_text->elide_behavior());

  gfx::Rect range_rect;
  for (const auto& rect : render_text->GetSubstringBounds(range))
    range_rect.Union(rect);
  return omnibox_view->GetLocalBounds().width() < range_rect.width();
}

}  // namespace

OmniboxViewViews::ElideAnimation::ElideAnimation(OmniboxViewViews* view,
                                                 gfx::RenderText* render_text)
    : AnimationDelegateViews(view), view_(view), render_text_(render_text) {
  DCHECK(view_);
  DCHECK(render_text_);
}

OmniboxViewViews::ElideAnimation::~ElideAnimation() = default;

// TODO(estark): this code doesn't work for URLs with RTL components. Will need
// to figure out another animation or just skip the animation entirely on URLs
// with RTL components.
void OmniboxViewViews::ElideAnimation::Start(
    const gfx::Range& elide_to_bounds,
    uint32_t delay_ms,
    const std::vector<gfx::Range>& ranges_surrounding_simplified_domain,
    SkColor starting_color,
    SkColor ending_color) {
  DCHECK(ranges_surrounding_simplified_domain.size() == 1 ||
         ranges_surrounding_simplified_domain.size() == 2);
  ranges_surrounding_simplified_domain_ = ranges_surrounding_simplified_domain;
  starting_color_ = starting_color;
  ending_color_ = ending_color;

  // simplified_domain_bounds_ will be set to a rectangle surrounding the part
  // of the URL that is never elided, on its original position before any
  // animation runs. If ranges_surrounding_simplified_domain_ only contains one
  // range it means we are not eliding on the right side, so we use the right
  // side of elide_to_bounds as the range as it will always be the right limit
  // of the simplified section.
  gfx::Range simplified_domain_range(
      ranges_surrounding_simplified_domain_[0].end(),
      ranges_surrounding_simplified_domain_.size() == 2
          ? ranges_surrounding_simplified_domain_[1].start()
          : elide_to_bounds.end());
  for (auto rect : render_text_->GetSubstringBounds(simplified_domain_range)) {
    simplified_domain_bounds_.Union(rect - render_text_->GetLineOffset(0));
  }

  // After computing |elide_to_rect_| below, |elide_to_bounds| aren't actually
  // need anymore for the animation. However, the bounds provide a convenient
  // way for the animation consumer to check if an animation is currently in
  // progress to a specific range, so that the consumer can avoid starting a
  // duplicate animation (to avoid flicker). So we save the bounds so that
  // consumers can query them.
  elide_to_bounds_ = elide_to_bounds;

  animation_ =
      std::make_unique<gfx::MultiAnimation>(gfx::MultiAnimation::Parts({
          gfx::MultiAnimation::Part(base::TimeDelta::FromMilliseconds(delay_ms),
                                    gfx::Tween::ZERO),
          gfx::MultiAnimation::Part(base::TimeDelta::FromMilliseconds(300),
                                    gfx::Tween::FAST_OUT_SLOW_IN),
      }));
  animation_->set_delegate(this);
  animation_->set_continuous(false);

  elide_from_rect_ = render_text_->display_rect();
  elide_to_rect_ = gfx::Rect();
  for (const auto& rect : render_text_->GetSubstringBounds(elide_to_bounds))
    elide_to_rect_.Union(rect);
  // The URL should never shift vertically while eliding to/from simplified
  // domain.
  elide_to_rect_.set_y(elide_from_rect_.y());
  elide_to_rect_.set_height(elide_from_rect_.height());

  // There is nothing to animate in this case, so return without starting.
  if (elide_from_rect_ == elide_to_rect_ && starting_color_ == ending_color_)
    return;

  starting_display_offset_ = render_text_->GetUpdatedDisplayOffset().x();
  // Shift the text to where |elide_to_bounds| starts, relative to the current
  // display rect.
  if (base::i18n::IsRTL()) {
    ending_display_offset_ = starting_display_offset_ +
                             elide_from_rect_.right() - elide_to_rect_.right();
  } else {
    ending_display_offset_ =
        starting_display_offset_ - (elide_to_rect_.x() - elide_from_rect_.x());
  }

  animation_->Start();
}

void OmniboxViewViews::ElideAnimation::Stop() {
  // Reset the smoothing rectangles whenever the animation stops to prevent
  // stale rectangles from showing at the start of the next animation.
  view_->elide_animation_smoothing_rect_left_ = gfx::Rect();
  view_->elide_animation_smoothing_rect_right_ = gfx::Rect();
  if (animation_)
    animation_->Stop();
}

bool OmniboxViewViews::ElideAnimation::IsAnimating() {
  return animation_ && animation_->is_animating();
}

const gfx::Range& OmniboxViewViews::ElideAnimation::GetElideToBounds() const {
  return elide_to_bounds_;
}

SkColor OmniboxViewViews::ElideAnimation::GetCurrentColor() const {
  return animation_
             ? gfx::Tween::ColorValueBetween(animation_->GetCurrentValue(),
                                             starting_color_, ending_color_)
             : gfx::kPlaceholderColor;
}

gfx::MultiAnimation*
OmniboxViewViews::ElideAnimation::GetAnimationForTesting() {
  return animation_.get();
}

void OmniboxViewViews::ElideAnimation::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK(!view_->model()->user_input_in_progress());
  DCHECK_EQ(animation, animation_.get());

  if (animation->GetCurrentValue() == 0)
    return;

  // |bounds| contains the interpolated substring to show for this frame. Shift
  // it to line up with the x position of the previous frame (|old_bounds|),
  // because the animation should gradually bring the desired string into view
  // at the leading edge. The y/height values shouldn't change because
  // |elide_to_rect_| is set to have the same y and height values as
  // |elide_to_rect_|.
  gfx::Rect old_bounds = render_text_->display_rect();
  gfx::Rect bounds = gfx::Tween::RectValueBetween(
      animation->GetCurrentValue(), elide_from_rect_, elide_to_rect_);
  DCHECK_EQ(bounds.y(), old_bounds.y());
  DCHECK_EQ(bounds.height(), old_bounds.height());
  gfx::Rect shifted_bounds(base::i18n::IsRTL()
                               ? old_bounds.right() - bounds.width()
                               : old_bounds.x(),
                           old_bounds.y(), bounds.width(), old_bounds.height());
  render_text_->SetDisplayRect(shifted_bounds);
  current_offset_ = gfx::Tween::IntValueBetween(animation->GetCurrentValue(),
                                                starting_display_offset_,
                                                ending_display_offset_);
  render_text_->SetDisplayOffset(current_offset_);

  for (const auto& range : ranges_surrounding_simplified_domain_) {
    view_->ApplyColor(GetCurrentColor(), range);
  }

  // TODO(crbug.com/1101472): The smoothing gradient mask is not yet implemented
  // correctly for RTL UI.
  if (base::i18n::IsRTL()) {
    view_->SchedulePaint();
    return;
  }

  // The gradient mask should be a fixed width, except if that width would
  // cause it to mask the unelided section. In that case we set it to the
  // maximum width possible that won't cover the unelided section.
  int unelided_left_bound = simplified_domain_bounds_.x() + current_offset_;
  int unelided_right_bound =
      unelided_left_bound + simplified_domain_bounds_.width();
  // GetSubstringBounds rounds up when calculating unelided_left_bound and
  // unelided_right_bound, we subtract 1 pixel from the gradient widths to make
  // sure they never overlap with the always visible part of the URL.
  // gfx::Rect() switches negative values to 0, so this doesn't affect
  // rectangles that were originally size 0.
  int left_gradient_width = kSmoothingGradientMaxWidth < unelided_left_bound
                                ? kSmoothingGradientMaxWidth - 1
                                : unelided_left_bound - 1;
  int right_gradient_width =
      shifted_bounds.right() - kSmoothingGradientMaxWidth > unelided_right_bound
          ? kSmoothingGradientMaxWidth - 1
          : shifted_bounds.right() - unelided_right_bound - 1;

  view_->elide_animation_smoothing_rect_left_ = gfx::Rect(
      old_bounds.x(), old_bounds.y(), left_gradient_width, old_bounds.height());
  view_->elide_animation_smoothing_rect_right_ =
      gfx::Rect(shifted_bounds.right() - right_gradient_width, old_bounds.y(),
                right_gradient_width, old_bounds.height());

  view_->SchedulePaint();
}

// OmniboxViewViews -----------------------------------------------------------

OmniboxViewViews::OmniboxViewViews(OmniboxEditController* controller,
                                   std::unique_ptr<OmniboxClient> client,
                                   bool popup_window_mode,
                                   LocationBarView* location_bar,
                                   const gfx::FontList& font_list)
    : OmniboxView(controller, std::move(client)),
      popup_window_mode_(popup_window_mode),
      clock_(base::DefaultClock::GetInstance()),
      location_bar_view_(location_bar),
      latency_histogram_state_(NOT_ACTIVE),
      friendly_suggestion_text_prefix_length_(0) {
  SetID(VIEW_ID_OMNIBOX);
  SetFontList(font_list);
  set_force_text_directionality(true);

  // Unit tests may use a mock location bar that has no browser,
  // or use no location bar at all.
  if (location_bar_view_ && location_bar_view_->browser()) {
    pref_change_registrar_.Init(
        location_bar_view_->browser()->profile()->GetPrefs());
    pref_change_registrar_.Add(
        omnibox::kPreventUrlElisionsInOmnibox,
        base::BindRepeating(&OmniboxViewViews::OnShouldPreventElisionChanged,
                            base::Unretained(this)));
  }

  // Sometimes there are additional ignored views, such as a View representing
  // the cursor, inside the address bar's text field. These should always be
  // ignored by accessibility since a plain text field should always be a leaf
  // node in the accessibility trees of all the platforms we support.
  GetViewAccessibility().OverrideIsLeaf(true);
}

OmniboxViewViews::~OmniboxViewViews() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  scoped_template_url_service_observation_.Observe(
      model()->client()->GetTemplateURLService());

  if (popup_window_mode_)
    SetReadOnly(true);

  if (location_bar_view_) {
    // Initialize the popup view using the same font.
    popup_view_ = std::make_unique<OmniboxPopupContentsView>(
        this, model(), location_bar_view_);
    if (OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction() &&
        !model()->ShouldPreventElision()) {
      Observe(location_bar_view_->GetWebContents());
    }

    // Set whether the text should be used to improve typing suggestions.
    SetShouldDoLearning(!location_bar_view_->profile()->IsOffTheRecord());
  }

  // Override the default FocusableBorder from Textfield, since the
  // LocationBarView will indicate the focus state.
  constexpr gfx::Insets kTextfieldInsets(3);
  SetBorder(views::CreateEmptyBorder(kTextfieldInsets));

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

  // When the tab is changed, unelide the URL in case it had previously been
  // elided to a simplified domain by a user interaction (when certain field
  // trials are enabled).
  ResetToHideOnInteraction();
  if (OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction() &&
      !model()->ShouldPreventElision()) {
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
    SetPlaceholderText(std::u16string());
  }
}

bool OmniboxViewViews::GetSelectionAtEnd() const {
  const gfx::Range sel = GetSelectedRange();
  return sel.GetMin() == GetText().size();
}

void OmniboxViewViews::EmphasizeURLComponents() {
  // Cancel any existing simplified URL animations.
  if (hover_elide_or_unelide_animation_)
    hover_elide_or_unelide_animation_->Stop();
  if (elide_after_web_contents_interaction_animation_)
    elide_after_web_contents_interaction_animation_->Stop();

  // If the current contents is a URL, turn on special URL rendering mode in
  // RenderText.
  bool text_is_url = model()->CurrentTextIsURL();
  GetRenderText()->SetDirectionalityMode(
      text_is_url ? gfx::DIRECTIONALITY_AS_URL : gfx::DIRECTIONALITY_FROM_TEXT);
  SetStyle(gfx::TEXT_STYLE_STRIKE, false);

  std::u16string text = GetText();
  UpdateTextStyle(text, text_is_url, model()->client()->GetSchemeClassifier());

  if (model()->ShouldPreventElision())
    return;

  // If the text isn't eligible to be elided to a simplified domain, and
  // simplified domain field trials are enabled, then ensure that as much of the
  // text as will fit is visible.
  if (!GetURLEligibleForSimplifiedDomainEliding() &&
      (OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction() ||
       OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover())) {
    FitToLocalBounds();
    return;
  }

  // In the simplified domain field trials, elide or unelide according to the
  // current state and field trial configuration. These elisions are not
  // animated because we often don't want this to be a user-visible
  // transformation; for example, a navigation should just show the URL in the
  // desired state without drawing additional attention from the user.
  if (OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction()) {
    // In the hide-on-interaction field trial, elide or unelide the URL to the
    // simplified domain depending on whether the user has already interacted
    // with the page or not. This is a best guess at the correct elision state,
    // which we don't really know for sure until a navigation has committed
    // (because the elision behavior depends on whether the navigation is
    // same-document and if it changes the path). We elide here based on the
    // current elision setting; we'll then update the elision state as we get
    // more information about the navigation in DidStartNavigation and
    // DidFinishNavigation.
    if (elide_after_web_contents_interaction_animation_) {
      // This can cause a slight quirk in browser-initiated navigations that
      // occur after the user interacts with the previous page. In this case,
      // the simplified domain will be shown briefly before we show the full URL
      // in DidStartNavigation().
      ElideURL();
    } else {
      // Note that here we are only adjusting the display of the URL, not
      // resetting any state associated with the animations (in particular, we
      // are not calling ResetToHideOnInteraction()). This is, as above, because
      // we don't know exactly how to set state until we know what kind of
      // navigation is happening. Thus here we are only adjusting the display so
      // things look right mid-navigation, and the final state will be set
      // appropriately in DidFinishNavigation().
      ShowFullURLWithoutSchemeAndTrivialSubdomain();
    }
  } else if (OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover()) {
    // If reveal-on-hover is enabled and hide-on-interaction is disabled, elide
    // to the simplified domain now.
    ElideURL();
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
    focus_reveal_lock.reset(
        BrowserView::GetBrowserViewForBrowser(location_bar_view_->browser())
            ->immersive_mode_controller()
            ->GetRevealedLock(ImmersiveModeController::ANIMATE_REVEAL_YES));
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
  if ((hover_elide_or_unelide_animation_ &&
       hover_elide_or_unelide_animation_->IsAnimating()) ||
      (elide_after_web_contents_interaction_animation_ &&
       elide_after_web_contents_interaction_animation_->IsAnimating())) {
    SkColor bg_color = GetBackgroundColor();
    // We can't use the SK_ColorTRANSPARENT constant here because for purposes
    // of the gradient the R,G,B values of the transparent color do matter, and
    // need to be identical to the background color (SK_ColorTRANSPARENT is a
    // transparent black, and results in the gradient looking gray).
    SkColor bg_transparent = SkColorSetARGB(
        0, SkColorGetR(bg_color), SkColorGetG(bg_color), SkColorGetB(bg_color));
    DrawGradientRect(elide_animation_smoothing_rect_left_, bg_color,
                     bg_transparent, canvas);
    DrawGradientRect(elide_animation_smoothing_rect_right_, bg_transparent,
                     bg_color, canvas);
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
  scoped_compositor_observation_.Observe(GetWidget()->GetCompositor());
}

void OmniboxViewViews::RemovedFromWidget() {
  views::Textfield::RemovedFromWidget();
  scoped_compositor_observation_.Reset();
}

OmniboxViewViews::ElideAnimation*
OmniboxViewViews::GetHoverElideOrUnelideAnimationForTesting() {
  return hover_elide_or_unelide_animation_.get();
}

OmniboxViewViews::ElideAnimation*
OmniboxViewViews::GetElideAfterInteractionAnimationForTesting() {
  return elide_after_web_contents_interaction_animation_.get();
}

void OmniboxViewViews::OnThemeChanged() {
  views::Textfield::OnThemeChanged();

  const SkColor dimmed_text_color = GetOmniboxColor(
      GetThemeProvider(), OmniboxPart::LOCATION_BAR_TEXT_DIMMED);
  set_placeholder_text_color(dimmed_text_color);

  if (!model()->ShouldPreventElision() &&
      OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover()) {
    hover_elide_or_unelide_animation_ =
        std::make_unique<ElideAnimation>(this, GetRenderText());
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

void OmniboxViewViews::ApplyColor(SkColor color, const gfx::Range& range) {
  Textfield::ApplyColor(color, range);
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
  static const uint32_t kPadTrailing = 30;
  static const uint32_t kPadLeading = 10;

  // We use SetTextWithoutCaretBoundsChangeNotification() in order to avoid
  // triggering accessibility events multiple times.
  SetTextWithoutCaretBoundsChangeNotification(text, ranges[0].end());
  Scroll({0, std::min<size_t>(ranges[0].end() + kPadTrailing, text.size()),
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

#if defined(OS_MAC)
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
    size_t user_text_length) {
  if (display_text == GetText())
    return;

  if (!IsIMEComposing()) {
    SetTextAndSelectedRanges(display_text, selections);
  } else if (location_bar_view_) {
    // TODO(manukh) IME should be updated with prefix and split rich
    // autocompletion if those features launch. Likewise, remove
    // |user_text_length| param if it can be computed.
    location_bar_view_->SetImeInlineAutocompletion(
        display_text.substr(user_text_length));
  }

  EmphasizeURLComponents();
}

void OmniboxViewViews::OnInlineAutocompleteTextCleared() {
  // Hide the inline autocompletion for IME users.
  if (location_bar_view_)
    location_bar_view_->SetImeInlineAutocompletion(std::u16string());
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
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
}

void OmniboxViewViews::SetAccessibilityLabel(const std::u16string& display_text,
                                             const AutocompleteMatch& match,
                                             bool notify_text_changed) {
  if (model()->popup_model()->selected_line() == OmniboxPopupModel::kNoMatch) {
    // If nothing is selected in the popup, we are in the no-default-match edge
    // case, and |match| is a synthetically generated match. In that case,
    // bypass OmniboxPopupModel and get the label from our synthetic |match|.
    friendly_suggestion_text_ = AutocompleteMatchType::ToAccessibilityLabel(
        match, display_text, OmniboxPopupModel::kNoMatch,
        model()->result().size(), std::u16string(),
        &friendly_suggestion_text_prefix_length_);
  } else {
    friendly_suggestion_text_ =
        model()->popup_model()->GetAccessibilityLabelForCurrentSelection(
            display_text, true, &friendly_suggestion_text_prefix_length_);
  }

  if (notify_text_changed)
    NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);

#if defined(OS_MAC)
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
    input_method->ShowVirtualKeyboardIfEnabled();
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

  if (model()->ShouldPreventElision())
    return;

  if (!GetURLEligibleForSimplifiedDomainEliding())
    return;

  if (hover_start_time_ == base::Time() &&
      GetURLEligibleForSimplifiedDomainEliding()) {
    hover_start_time_ = clock_->Now();
  }

  if (!OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover())
    return;

  if (elide_after_web_contents_interaction_animation_)
    elide_after_web_contents_interaction_animation_->Stop();

  // When the reveal-on-hover field trial is enabled, we elide the path and
  // optionally subdomains of the URL. We bring back the URL when the user
  // hovers over the omnibox, as is happening now. This is done via an animation
  // that slides both ends of the URL into view while shifting the text so that
  // the visible text is aligned with the leading edge of the display area. The
  // reverse animation occurs when the mouse exits the omnibox area (in
  // OnMouseExited()).
  //
  // The animation shouldn't begin immediately on hover to avoid the URL
  // flickering in and out as the user passes over the omnibox on their way to
  // e.g. the tab strip. Thus we pass a delay threshold (configurable via field
  // trial) to ElideAnimation so that the unelision animation only begins after
  // this delay.
  if (hover_elide_or_unelide_animation_) {
    // There might already be an unelide in progress. If it's animating to the
    // same state as we're targeting, then we don't need to do anything.
    gfx::Range unelide_bounds = gfx::Range(0, GetText().size());
    if (hover_elide_or_unelide_animation_->IsAnimating() &&
        hover_elide_or_unelide_animation_->GetElideToBounds() ==
            unelide_bounds) {
      return;
    }

    SkColor starting_color =
        hover_elide_or_unelide_animation_->GetCurrentColor();
    if (starting_color == gfx::kPlaceholderColor)
      starting_color = SK_ColorTRANSPARENT;
    hover_elide_or_unelide_animation_->Stop();

    // Figure out where we are uneliding from so that the hover animation can
    // fade in the surrounding text (|ranges_to_fade_in|). If the user has
    // already interacted with the page, then we elided to the simplified domain
    // and that is what we are uneliding from now. Otherwise, only the scheme
    // and possibly a trivial subdomain have been elided and those components
    // now need to be faded in.
    std::vector<gfx::Range> ranges_to_fade_in;
    // |minimum_visible_range| will contain either the simplified domain or the
    // full hostname, depending on which is currently supposed to be showing. If
    // |minimum_visible_range| does not currently fit in the omnibox bounds,
    // then we don't do any hover animation. This is for simplicity, because
    // ElideAnimation doesn't know how to position the text so that the most
    // important part of the hostname is showing if it doesn't all fit.
    // Furthermore, it doesn't seem necessary to do hover animation when the
    // hostname doesn't fit because nothing is being elided beyond what has to
    // be to fit in the local bounds.
    gfx::Range minimum_visible_range;
    if (elide_after_web_contents_interaction_animation_ ||
        !OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction()) {
      // The URL has been elided to the simplified domain. We want to fade in
      // everything surrounding the simplified domain.
      minimum_visible_range = GetSimplifiedDomainBounds(&ranges_to_fade_in);
    } else {
      // The full URL is showing, except for the scheme and trivial subdomain.
      // We want to fade in the scheme and trivial subdomain.
      url::Component host = GetHostComponentAfterTrivialSubdomain();
      ranges_to_fade_in.emplace_back(0, host.begin);
      minimum_visible_range = gfx::Range(host.begin, host.end());
    }

    if (TextRangeOverflowsView(this, GetRenderText(), minimum_visible_range))
      return;

    hover_elide_or_unelide_animation_->Start(
        unelide_bounds, OmniboxFieldTrial::UnelideURLOnHoverThresholdMs(),
        ranges_to_fade_in, starting_color,
        GetOmniboxColor(GetThemeProvider(),
                        OmniboxPart::LOCATION_BAR_TEXT_DIMMED));
  }
}

void OmniboxViewViews::OnMouseExited(const ui::MouseEvent& event) {
  if (location_bar_view_)
    location_bar_view_->OnOmniboxHovered(false);

  // A histogram records the duration that the user has hovered continuously
  // over the omnibox without focusing it.
  if (hover_start_time_ != base::Time() && !recorded_hover_on_focus_) {
    UmaHistogramTimes("Omnibox.HoverTime", clock_->Now() - hover_start_time_);
  }
  hover_start_time_ = base::Time();
  recorded_hover_on_focus_ = false;

  if (!OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover() ||
      model()->ShouldPreventElision()) {
    return;
  }
  if (!GetURLEligibleForSimplifiedDomainEliding())
    return;

  // When the reveal-on-hover field trial is enabled, we bring the URL into view
  // when the user hovers over the omnibox and elide back to simplified domain
  // when their mouse exits the omnibox area. The elision animation is the
  // reverse of the unelision animation: we shrink the URL from both sides while
  // shifting the text to the leading edge.
  DCHECK(hover_elide_or_unelide_animation_);
  SkColor starting_color =
      hover_elide_or_unelide_animation_->IsAnimating()
          ? hover_elide_or_unelide_animation_->GetCurrentColor()
          : GetOmniboxColor(GetThemeProvider(),
                            OmniboxPart::LOCATION_BAR_TEXT_DIMMED);
  hover_elide_or_unelide_animation_->Stop();
  // Elisions don't take display offset into account (see
  // https://crbug.com/1099078), so the RenderText must be in NO_ELIDE mode to
  // avoid over-eliding when some of the text is not visible due to display
  // offset.
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);

  // Figure out where to elide to. If the user has already interacted with the
  // page or reveal-on-interaction is disabled, then elide to the simplified
  // domain; otherwise just hide the scheme and trivial subdomain (if any).
  if (elide_after_web_contents_interaction_animation_ ||
      !OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction()) {
    std::vector<gfx::Range> ranges_surrounding_simplified_domain;
    gfx::Range simplified_domain =
        GetSimplifiedDomainBounds(&ranges_surrounding_simplified_domain);
    // If the simplified domain overflows the local bounds, then hover
    // animations are disabled for simplicity.
    if (TextRangeOverflowsView(this, GetRenderText(), simplified_domain))
      return;
    hover_elide_or_unelide_animation_->Start(
        simplified_domain, 0 /* delay_ms */,
        ranges_surrounding_simplified_domain, starting_color,
        SK_ColorTRANSPARENT);
  } else {
    std::u16string text = GetText();
    url::Component host = GetHostComponentAfterTrivialSubdomain();
    // If the hostname overflows the local bounds, then hover animations are
    // disabled for simplicity.
    if (TextRangeOverflowsView(this, GetRenderText(),
                               gfx::Range(host.begin, host.end()))) {
      return;
    }
    hover_elide_or_unelide_animation_->Start(
        gfx::Range(host.begin, text.size()), 0 /* delay_ms */,
        std::vector<gfx::Range>{gfx::Range(0, host.begin)}, starting_color,
        SK_ColorTRANSPARENT);
  }
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
  if (model()->popup_model() && model()->popup_model()->selected_line_state() !=
                                    OmniboxPopupModel::KEYWORD_MODE) {
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
      // the elided URL is selected prior to the dobule click. Unelision happens
      // between the first and second click, causing the wrong word to be
      // selected because it's based on the click position in the newly unelided
      // URL. See https://crbug.com/1084406.
      if (IsSelectAll()) {
        SelectWordAt(event.location());
        std::u16string shown_url = GetText();
        std::u16string full_url =
            controller()->GetLocationBarModel()->GetFormattedFullURL();
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
    model()->StartZeroSuggestRequest();

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
#if defined(OS_MAC)
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
    OmniboxResultView* selected_result_view =
        popup_view_->GetSelectedResultView();
    if (selected_result_view) {
      node_data->AddIntAttribute(
          ax::mojom::IntAttribute::kActivedescendantId,
          selected_result_view->GetViewAccessibility().GetUniqueId().Get());
    }
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

void OmniboxViewViews::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  Textfield::OnBoundsChanged(previous_bounds);

  if (!OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover() &&
      !OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction()) {
    return;
  }

  // When simplified domain display field trials are enabled,
  // Textfield::OnBoundsChanged() may have undone the effect of any previous URL
  // elisions, because it expands the Textfield's display rect to the local
  // bounds, which may bring more of the URL into view than intended. Re-apply
  // simplified domain elisions now.

  // Cancel any running animations. This could cause some abrupt transitions,
  // but we can't adapt running animations to new bounds.
  if (hover_elide_or_unelide_animation_)
    hover_elide_or_unelide_animation_->Stop();
  if (elide_after_web_contents_interaction_animation_)
    elide_after_web_contents_interaction_animation_->Stop();

  // |elide_after_web_contents_interaction_animation_| is created when the user
  // interacts with the page, if hide-on-interaction is enabled. If
  // hide-on-interaction is disabled or the user has already interacted with the
  // page, the simplified domain should have been showing before the bounds
  // changed (or we would have been in the process of animating to the
  // simplified domain).
  if (!OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction() ||
      elide_after_web_contents_interaction_animation_) {
    if (GetURLEligibleForSimplifiedDomainEliding() &&
        !model()->ShouldPreventElision()) {
      ElideURL();
    }
  } else {
    // The user hasn't interacted with the page yet. This resets animation state
    // and shows the partially elided URL with scheme and trivial subdomains
    // hidden.
    ResetToHideOnInteraction();
  }
}

void OmniboxViewViews::OnFocus() {
  views::Textfield::OnFocus();

  // A histogram records the duration that the user has hovered continuously
  // over the omnibox without focusing it.
  if (hover_start_time_ != base::Time() && !recorded_hover_on_focus_) {
    recorded_hover_on_focus_ = true;
    UmaHistogramTimes("Omnibox.HoverTime", clock_->Now() - hover_start_time_);
  }

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

  ShowFullURL();
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);

  // Focus changes can affect the visibility of any keyword hint.
  if (location_bar_view_ && model()->is_keyword_hint())
    location_bar_view_->Layout();

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

  // When the relevant field trial is enabled, reset state so that the URL will
  // be elided/unelided on next user interaction or hover.
  if (!model()->ShouldPreventElision()) {
    if (OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover() &&
        !OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction()) {
      // When reveal-on-hover is enabled but not hide-on-interaction, blur
      // should unfocus the omnibox and return to the same state as on page
      // load: the URL is elided to a simplified domain until the user hovers
      // over the omnibox. There's no need to animate in this case because the
      // omnibox's appearance already changes quite dramatically on blur
      // (selection clearer, other URL transformations, etc.), so there's no
      // need to make this change gradual.
      hover_elide_or_unelide_animation_ =
          std::make_unique<OmniboxViewViews::ElideAnimation>(this,
                                                             GetRenderText());
      if (GetURLEligibleForSimplifiedDomainEliding()) {
        ElideURL();
      } else {
        // If the text isn't eligible to be elided to a simplified domain, then
        // ensure that as much of it is visible as will fit.
        FitToLocalBounds();
      }
    } else if (OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction()) {
      // When hide-on-interaction is enabled, this method ensures that, once the
      // omnibox is blurred, the URL is visible and that the animation state is
      // set so that the URL will be animated to the simplified domain the
      // next time the user interacts with the page.
      ResetToHideOnInteraction();
    }
  }
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

  // Menu item is only shown when it is valid.
  if (command_id == IDC_SHOW_FULL_URLS)
    return true;

  return Textfield::IsCommandIdEnabled(command_id) ||
         location_bar_view_->command_updater()->IsCommandEnabled(command_id);
}

void OmniboxViewViews::DidStartNavigation(
    content::NavigationHandle* navigation) {
  if (!OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction() ||
      model()->ShouldPreventElision()) {
    return;
  }

  // If navigating to a different page in a browser-initiated navigation, the
  // new URL should be shown unelided while the navigation is in progress. For
  // renderer-initiated navigations, the URL isn't displayed until the
  // navigation commits, so there's no need to elide/unelide it now.
  if (navigation->IsInMainFrame() && !navigation->IsSameDocument() &&
      !navigation->IsRendererInitiated()) {
    ResetToHideOnInteraction();
  }
}

void OmniboxViewViews::DidFinishNavigation(
    content::NavigationHandle* navigation) {
  if (!OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction() ||
      model()->ShouldPreventElision()) {
    return;
  }

  // Non-main-frame navigations don't change the visible URL, so no action is
  // necessary for simplified domain elisions.
  if (!navigation->IsInMainFrame())
    return;

  // If the navigation didn't commit, and it was renderer-initiated, then no
  // action is needed, as the URL won't have been updated. But if it was
  // browser-initiated, then the URL would have been updated to show the URL of
  // the in-progress navigation; in this case, reset to show the full URL now
  // that the navigation has finished without committing.
  if (!navigation->HasCommitted()) {
    if (navigation->IsRendererInitiated()) {
      return;
    }
    ResetToHideOnInteraction();
    return;
  }

  // Once a navigation finishes that changes the visible URL (besides just the
  // ref), unelide and reset state so that we'll show the simplified domain on
  // interaction. Same-document navigations that only change the ref are treated
  // specially and don't cause the elision/unelision state to be altered. This
  // is to avoid frequent eliding/uneliding within single-page apps that do
  // frequent fragment navigations.
  if (navigation->IsErrorPage() || !navigation->IsSameDocument() ||
      !navigation->GetPreviousMainFrameURL().EqualsIgnoringRef(
          navigation->GetURL())) {
    ResetToHideOnInteraction();
  }
}

void OmniboxViewViews::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  // Exclude mouse clicks from triggering the simplified domain elision. Mouse
  // clicks can be done idly and aren't a good signal of real intent to interact
  // with the page. Plus, it can be jarring when the URL elides when the user
  // clicks on a link only to immediately come back as the navigation occurs.
  if (blink::WebInputEvent::IsMouseEventType(event.GetType()))
    return;

  // Exclude modifier keys to prevent keyboard shortcuts (such as switching
  // tabs) from eliding the URL. We don't want to count these shortcuts as
  // interactions with the page content.
  if (blink::WebInputEvent::IsKeyboardEventType(event.GetType()) &&
      event.GetModifiers() & blink::WebInputEvent::kKeyModifiers) {
    return;
  }

  MaybeElideURLWithAnimationFromInteraction();
}

void OmniboxViewViews::OnFocusChangedInPage(
    content::FocusedNodeDetails* details) {
  // Elide the URL to the simplified domain (the most security-critical
  // information) when the user focuses a form text field, which is a key moment
  // for making security decisions. Ignore the focus event if it didn't come
  // from a mouse click/tap. Focus via keyboard will trigger elision from
  // DidGetUserInteraction(), and we want to ignore focuses that aren't from an
  // explicit user action (e.g., input fields that are autofocused on page
  // load).
  if (details->is_editable_node &&
      details->focus_type == blink::mojom::FocusType::kMouse) {
    MaybeElideURLWithAnimationFromInteraction();
  }
}

std::u16string OmniboxViewViews::GetSelectionClipboardText() const {
  return SanitizeTextForPaste(Textfield::GetSelectionClipboardText());
}

void OmniboxViewViews::DoInsertChar(char16_t ch) {
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
                                       const std::u16string& new_contents) {}

bool OmniboxViewViews::HandleKeyEvent(views::Textfield* textfield,
                                      const ui::KeyEvent& event) {
  PermitExternalProtocolHandler();

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
      OmniboxPopupModel* popup_model = model()->popup_model();
      if (popup_model && popup_model->TriggerSelectionAction(
                             popup_model->selection(), event.time_stamp())) {
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
        model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB,
                             event.time_stamp());
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
      OmniboxPopupModel* popup_model = model()->popup_model();
      if (popup_model && !control && !alt && !shift &&
          popup_model->selection().IsButtonFocused()) {
        if (popup_model->TriggerSelectionAction(popup_model->selection(),
                                                event.time_stamp())) {
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
      ui::EndpointType::kDefault, /*notify_if_restricted=*/false);
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
                                          nullptr, *GetWidget(), data);
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

DragOperation OmniboxViewViews::OnDrop(const ui::OSExchangeData& data) {
  if (HasTextBeingDragged())
    return DragOperation::kNone;

  std::u16string text;
  if (data.HasURL(ui::FilenameToURLPolicy::CONVERT_FILENAMES)) {
    GURL url;
    std::u16string title;
    if (data.GetURLAndTitle(ui::FilenameToURLPolicy::CONVERT_FILENAMES, &url,
                            &title)) {
      text = StripJavascriptSchemas(base::UTF8ToUTF16(url.spec()));
    }
  } else if (data.HasString() && data.GetString(&text)) {
    text = StripJavascriptSchemas(base::CollapseWhitespace(text, true));
  } else {
    return DragOperation::kNone;
  }

  SetUserText(text);
  if (!HasFocus())
    RequestFocus();
  SelectAll(false);
  return DragOperation::kCopy;
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
#if !defined(OS_MAC)
    menu_contents->SetIcon(index,
                           ui::ImageModel::FromVectorIcon(kSendTabToSelfIcon));
#endif
    menu_contents->InsertSeparatorAt(++index, ui::NORMAL_SEPARATOR);
  }

  int paste_position = menu_contents->GetIndexOfCommandId(Textfield::kPaste);
  DCHECK_GE(paste_position, 0);
  menu_contents->InsertItemWithStringIdAt(paste_position + 1, IDC_PASTE_AND_GO,
                                          IDS_PASTE_AND_GO);

  menu_contents->AddSeparator(ui::NORMAL_SEPARATOR);

  menu_contents->AddItemWithStringId(IDC_EDIT_SEARCH_ENGINES,
                                     IDS_EDIT_SEARCH_ENGINES);

  const PrefService::Preference* show_full_urls_pref =
      location_bar_view_->profile()->GetPrefs()->FindPreference(
          omnibox::kPreventUrlElisionsInOmnibox);
  if (!show_full_urls_pref->IsManaged()) {
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
  scoped_compositor_observation_.Reset();
}

void OmniboxViewViews::OnTemplateURLServiceChanged() {
  InstallPlaceholderText();
}

void OmniboxViewViews::PermitExternalProtocolHandler() {
  ExternalProtocolHandler::PermitLaunchUrl();
}

gfx::Range OmniboxViewViews::GetSimplifiedDomainBounds(
    std::vector<gfx::Range>* ranges_surrounding_simplified_domain) {
  DCHECK(ranges_surrounding_simplified_domain);
  DCHECK(ranges_surrounding_simplified_domain->empty());

  std::u16string text = GetText();
  url::Component host = GetHostComponentAfterTrivialSubdomain();

  GURL url = url_formatter::FixupURL(base::UTF16ToUTF8(text), std::string());
  if (!OmniboxFieldTrial::ShouldMaybeElideToRegistrableDomain() ||
      !ShouldElideToRegistrableDomain(url)) {
    ranges_surrounding_simplified_domain->emplace_back(0, host.begin);
    ranges_surrounding_simplified_domain->emplace_back(host.end(), text.size());
    return gfx::Range(host.begin, host.end());
  }

  // TODO(estark): push this inside ParseForEmphasizeComponents()?
  std::u16string simplified_domain = url_formatter::IDNToUnicode(
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES));

  if (simplified_domain.empty()) {
    ranges_surrounding_simplified_domain->emplace_back(0, host.begin);
    ranges_surrounding_simplified_domain->emplace_back(host.end(), text.size());
    return gfx::Range(host.begin, host.end());
  }

  size_t simplified_domain_pos = text.rfind(simplified_domain, host.end());
  DCHECK_NE(simplified_domain_pos, std::string::npos);
  ranges_surrounding_simplified_domain->emplace_back(0, simplified_domain_pos);
  ranges_surrounding_simplified_domain->emplace_back(host.end(), text.size());
  return gfx::Range(simplified_domain_pos, host.end());
}

bool OmniboxViewViews::GetURLEligibleForSimplifiedDomainEliding() const {
  if (HasFocus() || model()->user_input_in_progress())
    return false;
  if (!model()->CurrentTextIsURL())
    return false;
  std::u16string text = GetText();
  url::Parsed parts;
  std::u16string scheme_str;
  // Call Parse() here instead of ParseForEmphasizeComponents() because the
  // latter parses the inner URL for blob:, filesystem:, and view-source: URLs.
  // For those schemes, we want the outer scheme so that we can disable elision
  // for those schemes.
  AutocompleteInput::Parse(text, std::string(),
                           model()->client()->GetSchemeClassifier(), &parts,
                           &scheme_str, nullptr);

  // TODO(crbug.com/1117631): Simplified domain elision can have bugs for some
  // URLs with bidirectional hosts, disable elision for those URLs while the
  // bugs are fixed.
  const std::u16string url_host = text.substr(parts.host.begin, parts.host.len);
  if (base::i18n::GetStringDirection(url_host) ==
      base::i18n::TextDirection::UNKNOWN_DIRECTION) {
    return false;
  }

  // Simplified domain display only makes sense for http/https schemes; for now
  // we don't want to mess with the display of other URLs like data:, blob:,
  // chrome:, etc.
  return (scheme_str == base::UTF8ToUTF16(url::kHttpScheme) ||
          scheme_str == base::UTF8ToUTF16(url::kHttpsScheme)) &&
         !url_host.empty() &&
         !net::HostStringIsLocalhost(
             base::UTF16ToUTF8(text.substr(parts.host.begin, parts.host.len)));
}

void OmniboxViewViews::ResetToHideOnInteraction() {
  if (!OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction() ||
      model()->ShouldPreventElision()) {
    return;
  }
  // Delete the interaction animation; it'll get recreated in
  // DidGetUserInteraction(). Recreate the hover animation now because the user
  // can hover over the URL before interacting with the page to reveal the
  // scheme and trivial subdomain (if any).
  elide_after_web_contents_interaction_animation_.reset();
  hover_elide_or_unelide_animation_ =
      std::make_unique<OmniboxViewViews::ElideAnimation>(this, GetRenderText());
  if (GetURLEligibleForSimplifiedDomainEliding()) {
    ShowFullURLWithoutSchemeAndTrivialSubdomain();
  } else {
    if (!HasFocus() && !model()->user_input_in_progress())
      GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
    FitToLocalBounds();
  }
}

void OmniboxViewViews::OnShouldPreventElisionChanged() {
  Update();
  if (!OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction() &&
      !OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover()) {
    return;
  }
  if (model()->ShouldPreventElision()) {
    hover_elide_or_unelide_animation_.reset();
    elide_after_web_contents_interaction_animation_.reset();
    if (GetURLEligibleForSimplifiedDomainEliding())
      ShowFullURL();
    return;
  }
  if (OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction()) {
    if (location_bar_view_)
      Observe(location_bar_view_->GetWebContents());
    ResetToHideOnInteraction();
  } else if (OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover()) {
    if (GetURLEligibleForSimplifiedDomainEliding()) {
      ElideURL();
    }
    hover_elide_or_unelide_animation_ =
        std::make_unique<ElideAnimation>(this, GetRenderText());
  }
}

void OmniboxViewViews::MaybeElideURLWithAnimationFromInteraction() {
  if (!OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction() ||
      model()->ShouldPreventElision()) {
    return;
  }

  // If there's already a hover animation running, just let it run as we will
  // end up at the same place.
  if (hover_elide_or_unelide_animation_->IsAnimating())
    return;

  // This method runs when the user interacts with the page, such as scrolling
  // or typing. In the hide-on-interaction field trial, the URL is shown until
  // user interaction, at which point it's animated to a simplified version of
  // the domain (hiding the path and, optionally, subdomains). The animation is
  // designed to draw the user's attention and suggest that they can return to
  // the omnibox to uncover the full URL.

  // If we've already created and run the animation in an earlier call to this
  // method, we don't need to do so again.
  if (!GetURLEligibleForSimplifiedDomainEliding() ||
      elide_after_web_contents_interaction_animation_) {
    return;
  }
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);
  elide_after_web_contents_interaction_animation_ =
      std::make_unique<ElideAnimation>(this, GetRenderText());
  std::vector<gfx::Range> ranges_surrounding_simplified_domain;
  gfx::Range simplified_domain =
      GetSimplifiedDomainBounds(&ranges_surrounding_simplified_domain);
  elide_after_web_contents_interaction_animation_->Start(
      simplified_domain, 0 /* delay_ms */, ranges_surrounding_simplified_domain,
      GetOmniboxColor(GetThemeProvider(),
                      OmniboxPart::LOCATION_BAR_TEXT_DIMMED),
      SK_ColorTRANSPARENT);
}

void OmniboxViewViews::ElideURL() {
  DCHECK(OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction() ||
         OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover());
  DCHECK(GetURLEligibleForSimplifiedDomainEliding());

  std::vector<gfx::Range> ranges_surrounding_simplified_domain;
  gfx::Range simplified_domain_bounds =
      GetSimplifiedDomainBounds(&ranges_surrounding_simplified_domain);

  // Setting the elision behavior to anything other than NO_ELIDE would result
  // in the string getting cut off shorter the simplified domain, because
  // display offset isn't taken into account when RenderText elides the string.
  // See https://crbug.com/1099078. It's important to set to NO_ELIDE before
  // starting to calculate simplified domain bounds with GetSubstringBounds(),
  // because GetSubstringBounds() will fail if the simplified domain isn't
  // visible due to RenderText elision.
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);

  // The simplified domain string must be a substring of the current display
  // text in order to elide to it.
  DCHECK_NE(
      GetRenderText()->GetDisplayText().find(GetText().substr(
          simplified_domain_bounds.start(), simplified_domain_bounds.end())),
      std::string::npos);

  SetCursorEnabled(false);

  gfx::Rect simplified_domain_rect;
  for (const auto& rect :
       GetRenderText()->GetSubstringBounds(simplified_domain_bounds)) {
    simplified_domain_rect.Union(rect);
  }

  // |simplified_domain_rect| gives us the current bounds of the simplified
  // domain substring. We shift it to the leftmost (rightmost if UI is RTL) edge
  // of the omnibox (as determined by the x position of the current display
  // rect), and then scroll to where the simplified domain begins, so that the
  // simplified domain appears at the leftmost/rightmost edge.
  gfx::Rect old_bounds = GetRenderText()->display_rect();
  int shifted_simplified_domain_x_pos;
  // The x position of the elided domain will depend on whether the UI is LTR or
  // RTL.
  if (base::i18n::IsRTL()) {
    shifted_simplified_domain_x_pos =
        old_bounds.right() - simplified_domain_rect.width();
  } else {
    shifted_simplified_domain_x_pos = old_bounds.x();
  }
  // Use |old_bounds| for y and height values because the URL should never shift
  // vertically while eliding to/from simplified domain.
  gfx::Rect shifted_simplified_domain_rect(
      shifted_simplified_domain_x_pos, old_bounds.y(),
      simplified_domain_rect.width(), old_bounds.height());

  // Now apply the display rect and offset so that exactly the simplified domain
  // is visible.

  // First check if the simplified domain fits in the local bounds. If it
  // doesn't, then we need to scroll so that the rightmost side is visible (e.g.
  // "evil.com" instead of "victim.com" if the full hostname
  // "victim.com.evil.com"). This check is only necessary for LTR mode because
  // in RTL mode, we scroll to the rightmost side of the domain automatically.
  if (shifted_simplified_domain_rect.width() > GetLocalBounds().width() &&
      !base::i18n::IsRTL()) {
    FitToLocalBounds();
    GetRenderText()->SetDisplayOffset(
        GetRenderText()->GetUpdatedDisplayOffset().x() -
        (simplified_domain_rect.right() -
         GetRenderText()->display_rect().width()));
  } else {
    // The simplified domain fits in the local bounds, so we proceed to set the
    // display rect and offset to make the simplified domain visible.
    GetRenderText()->SetDisplayRect(shifted_simplified_domain_rect);
    // Scroll the text to where the simplified domain begins, relative to the
    // leftmost (rightmost if UI is RTL) edge of the current display rect.
    if (base::i18n::IsRTL()) {
      GetRenderText()->SetDisplayOffset(
          GetRenderText()->GetUpdatedDisplayOffset().x() + old_bounds.right() -
          simplified_domain_rect.right());
    } else {
      GetRenderText()->SetDisplayOffset(
          GetRenderText()->GetUpdatedDisplayOffset().x() -
          (simplified_domain_rect.x() - old_bounds.x()));
    }
  }

  // GetSubstringBounds() rounds outward internally, so there may be small
  // portions of text still showing. Set the ranges surrounding the simplified
  // domain to transparent so that these artifacts don't show.
  for (const auto& range : ranges_surrounding_simplified_domain)
    ApplyColor(SK_ColorTRANSPARENT, range);
}

void OmniboxViewViews::ShowFullURL() {
  if (!OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction() &&
      !OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover()) {
    return;
  }

  if (hover_elide_or_unelide_animation_)
    hover_elide_or_unelide_animation_->Stop();
  if (elide_after_web_contents_interaction_animation_)
    elide_after_web_contents_interaction_animation_->Stop();
  ApplyCaretVisibility();
  FitToLocalBounds();

  // Previous animations or elisions might have faded the path and/or subdomains
  // to transparent, so reset their color now that they should be visible.
  ApplyColor(GetOmniboxColor(GetThemeProvider(),
                             OmniboxPart::LOCATION_BAR_TEXT_DIMMED),
             gfx::Range(0, GetText().size()));
  UpdateTextStyle(GetText(), model()->CurrentTextIsURL(),
                  model()->client()->GetSchemeClassifier());

  GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
}

void OmniboxViewViews::ShowFullURLWithoutSchemeAndTrivialSubdomain() {
  DCHECK(GetURLEligibleForSimplifiedDomainEliding());
  DCHECK(OmniboxFieldTrial::ShouldHidePathQueryRefOnInteraction() ||
         OmniboxFieldTrial::ShouldRevealPathQueryRefOnHover());
  DCHECK(!model()->ShouldPreventElision());

  // First show the full URL, then figure out what to elide.
  ShowFullURL();

  if (!GetURLEligibleForSimplifiedDomainEliding() ||
      model()->ShouldPreventElision()) {
    return;
  }

  // TODO(https://crbug.com/1099078): currently, we cannot set the elide
  // behavior to anything other than NO_ELIDE when the display offset is 0, i.e.
  // when we are not hiding the scheme and trivial subdomain. This is because
  // RenderText does not take display offset into account when eliding, so it
  // will over-elide by however much text is scrolled out of the display area.
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);

  GetRenderText()->SetDisplayOffset(0);
  const gfx::Rect& current_display_rect = GetRenderText()->display_rect();

  // If the scheme and trivial subdomain should be elided, then we want to set
  // the display offset to where the hostname after the trivial subdomain (if
  // any) begins, relative to the current display rect.
  std::u16string text = GetText();
  url::Component host = GetHostComponentAfterTrivialSubdomain();

  // First check if the full hostname can fit in the local bounds. If not, then
  // show the rightmost portion of the hostname.
  gfx::Rect display_url_bounds;
  gfx::Range host_range(host.begin, host.end());
  if (TextRangeOverflowsView(this, GetRenderText(), host_range)) {
    gfx::Rect host_bounds;
    for (const auto& rect : GetRenderText()->GetSubstringBounds(host_range))
      host_bounds.Union(rect);
    // The full hostname won't fit, so show as much of it as possible starting
    // from the right side.
    display_url_bounds.set_x(
        current_display_rect.x() +
        (host_bounds.right() - current_display_rect.right()));
    display_url_bounds.set_y(current_display_rect.y());
    display_url_bounds.set_width(current_display_rect.width());
    display_url_bounds.set_height(current_display_rect.height());
  } else {
    for (const auto& rect : GetRenderText()->GetSubstringBounds(
             gfx::Range(host.begin, text.size()))) {
      display_url_bounds.Union(rect);
    }
    display_url_bounds.set_height(current_display_rect.height());
    display_url_bounds.set_y(current_display_rect.y());
  }

  // Set the scheme and trivial subdomain to transparent. This isn't necessary
  // to hide this portion of the text because it will be scrolled out of
  // visibility anyway when we set the display offset below. However, if the
  // user subsequently hovers over the URL to bring back the scheme and trivial
  // subdomain, the hover animation assumes that the hidden text starts from
  // transparent and fades it back in.
  ApplyColor(SK_ColorTRANSPARENT, gfx::Range(0, host.begin));

  // Before setting the display offset, set the display rect to the portion of
  // the URL that won't be elided, or leave it at the local bounds, whichever is
  // smaller. The display offset is capped at 0 if the text doesn't overflow the
  // display rect, so we must fit the display rect to the text so that we can
  // then set the display offset to scroll the scheme and trivial subdomain out
  // of visibility.
  GetRenderText()->SetDisplayRect(
      gfx::Rect(base::i18n::IsRTL()
                    ? current_display_rect.right() - display_url_bounds.width()
                    : current_display_rect.x(),
                display_url_bounds.y(), display_url_bounds.width(),
                display_url_bounds.height()));

  GetRenderText()->SetDisplayOffset(
      -1 * (display_url_bounds.x() - current_display_rect.x()));
}

url::Component OmniboxViewViews::GetHostComponentAfterTrivialSubdomain() const {
  url::Component host;
  url::Component unused_scheme;
  std::u16string text = GetText();
  AutocompleteInput::ParseForEmphasizeComponents(
      text, model()->client()->GetSchemeClassifier(), &unused_scheme, &host);
  url_formatter::StripWWWFromHostComponent(base::UTF16ToUTF8(text), &host);
  return host;
}

BEGIN_METADATA(OmniboxViewViews, views::Textfield)
ADD_READONLY_PROPERTY_METADATA(bool, SelectionAtEnd)
ADD_READONLY_PROPERTY_METADATA(int, TextWidth)
ADD_READONLY_PROPERTY_METADATA(int, UnelidedTextWidth)
ADD_READONLY_PROPERTY_METADATA(int, Width)
ADD_READONLY_PROPERTY_METADATA(std::u16string, SelectedText)
ADD_READONLY_PROPERTY_METADATA(bool, URLEligibleForSimplifiedDomainEliding)
ADD_READONLY_PROPERTY_METADATA(url::Component,
                               HostComponentAfterTrivialSubdomain)
END_METADATA
