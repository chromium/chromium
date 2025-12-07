// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic_button.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace glic {

// TODO(crbug.com/461326322): Remove this flag when crbug.com/461326322 is
// resolved.
BASE_FEATURE(kGlicButtonHideLabelOnTaskNudge, base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

constexpr int kHighlightMargin = 2;
constexpr int kHighlightCornerRadius = 8;
constexpr int kLabelRightMargin = 8;
constexpr int kCloseButtonMargin = 6;
constexpr ui::ColorId kHighlightColorId = ui::kColorSysPrimary;
constexpr ui::ColorId kTextOnHighlight = ui::kColorSysOnPrimary;
constexpr ui::ColorId kTextDisabledOnHighlight = kTextOnHighlight;
constexpr ui::ColorId kTextDisabled = ui::kColorLabelForegroundDisabled;

constexpr ui::ColorId kForeground = kColorNewTabButtonForegroundFrameActive;
constexpr ui::ColorId kForegroundOnAltBackground = ui::kColorSysOnSurface;

// TODO(crbug.com/453739403): Update with final color IDs.
constexpr ui::ColorId kBackgroundWhenGlicOpenActive =
    ui::kColorSysStateHeaderHover;
constexpr ui::ColorId kBackgroundWhenGlicOpenInactive =
    ui::kColorSysStateDisabledContainer;

constexpr int kIconSize = 16;
// TODO(crbug.com/460400955): Move this constant to a shared location.
// This should mirror the tween used for TabStripNudgeAnimationSession.
constexpr gfx::Tween::Type kSlidingTextTween =
    gfx::Tween::Type::ACCEL_20_DECEL_100;

constexpr int kMinTargetWidthForAnimatingText = 41;

bool EntrypointVariationsEnabled() {
  return base::FeatureList::IsEnabled(features::kGlicEntrypointVariations);
}

bool ShouldShowLabel() {
  return EntrypointVariationsEnabled() &&
         features::kGlicEntrypointVariationsShowLabel.Get();
}

std::u16string GetLabelText() {
  return ShouldShowLabel()
             ? l10n_util::GetStringUTF16(IDS_GLIC_BUTTON_ENTRYPOINT_LABEL)
             : std::u16string();
}

bool ShouldUseAltIcon() {
  return EntrypointVariationsEnabled() &&
         features::kGlicEntrypointVariationsAltIcon.Get();
}

bool HighlightNudgeEnabled() {
  return EntrypointVariationsEnabled() &&
         features::kGlicEntrypointVariationsHighlightNudge.Get();
}

const gfx::VectorIcon& GlicVectorIcon() {
  return glic::GlicVectorIconManager::GetVectorIcon(
      IDR_GLIC_BUTTON_VECTOR_ICON);
}

ui::ImageModel GetNormalIcon() {
  if (ShouldUseAltIcon()) {
    return ui::ImageModel::FromImageSkia(
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_GLIC_BUTTON_ALT_ICON));
  }
  return ui::ImageModel::FromVectorIcon(
      GlicVectorIcon(),
      ShouldUseAltIcon() ? kForegroundOnAltBackground : kForeground, kIconSize);
}

ui::ImageModel GetIconForHighlight() {
  if (HighlightNudgeEnabled()) {
    return ui::ImageModel::FromVectorIcon(GlicVectorIcon(), kTextOnHighlight,
                                          kIconSize);
  }
  return {};
}

gfx::Insets GetIconMargins() {
  int left = 6 - kHighlightMargin;
  int right = 4;

  if (ShouldShowLabel()) {
    // Extra left margin if the label is shown.
    left += 2;
  }
  return gfx::Insets().set_left_right(left, right);
}

// Helper for making animation durations instant if animations are disabled.
base::TimeDelta DurationMs(int duration_ms) {
  return gfx::Animation::ShouldRenderRichAnimation()
             ? base::Milliseconds(duration_ms)
             : base::TimeDelta();
}

}  // namespace

GlicButton::GlicButton(TabStripController* tab_strip_controller,
                       PressedCallback pressed_callback,
                       PressedCallback close_pressed_callback,
                       base::RepeatingClosure hovered_callback,
                       base::RepeatingClosure mouse_down_callback,
                       base::RepeatingClosure expansion_animation_done_callback,
                       const std::u16string& tooltip)
    : TabStripNudgeButton(tab_strip_controller,
                          std::move(pressed_callback),
                          std::move(close_pressed_callback),
                          GetLabelText(),
                          kGlicNudgeButtonElementId,
                          Edge::kNone,
                          gfx::VectorIcon::EmptyIcon(),
                          /*show_close_button=*/true),
      menu_model_(CreateMenuModel()),
      tab_strip_controller_(tab_strip_controller),
      hovered_callback_(std::move(hovered_callback)),
      mouse_down_callback_(std::move(mouse_down_callback)),
      expansion_animation_done_callback_(
          std::move(expansion_animation_done_callback)),
      normal_icon_(GetNormalIcon()),
      icon_for_highlight_(GetIconForHighlight()) {
  SetProperty(views::kElementIdentifierKey, kGlicButtonElementId);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  UpdateIcon();
  auto* image_view = static_cast<views::ImageView*>(image_container_view());
  image_view->SetImageSize({kIconSize, kIconSize});
  image_view->SetProperty(views::kMarginsKey, GetIconMargins());
  image_view->SetPaintToLayer();
  image_view->layer()->SetFillsBoundsOpaquely(false);

  CreateIconAndLabelContainer();

  if (!label()->layer()) {
    // Make sure label() has a layer even if its text is empty, so we can use
    // the same opacity animation whether or not the label has text.
    label()->SetPaintToLayer();
  }
  label()->SetProperty(views::kMarginsKey,
                       gfx::Insets().set_right(kLabelRightMargin));
  close_button()->SetProperty(
      views::kMarginsKey, gfx::Insets().set_left_right(
                              HighlightNudgeEnabled() ? kCloseButtonMargin : 0,
                              kCloseButtonMargin));
  SetCloseButtonVisible(false);

  set_context_menu_controller(this);

  SetTooltipText(tooltip);
  GetViewAccessibility().SetName(tooltip);

  SetDefaultColors();
  UpdateColors();
  SetVisible(true);

  SetFocusBehavior(FocusBehavior::ALWAYS);

  auto* const layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  // Subscribe to changes in state of glic FRE dialog and glic window.
  glic::GlicKeyedService* service =
      glic::GlicKeyedService::Get(tab_strip_controller_->GetProfile());
  glic_window_activation_subscription_ =
      service->window_controller().AddWindowActivationChangedCallback(
          base::BindRepeating(&GlicButton::PanelStateChanged,
                              base::Unretained(this)));

  fre_subscription_ = service->fre_controller().AddWebUiStateChangedCallback(
      base::BindRepeating(&GlicButton::OnFreWebUiStateChanged,
                          base::Unretained(this)));
}

GlicButton::~GlicButton() = default;

// Static
GlicButton* GlicButton::FromBrowser(BrowserWindowInterface* browser) {
  if (!browser) {
    return nullptr;
  }
  return BrowserElementsViews::From(browser)->GetViewAs<glic::GlicButton>(
      kGlicButtonElementId);
}

void GlicButton::SetNudgeLabel(std::string label) {
  if (!EntrypointVariationsEnabled()) {
    initial_width_ = GetLayoutManager()->GetPreferredSize(this).width();
    return SetText(base::UTF8ToUTF16(label));
  }
  // Store the new label text until the right moment in the animation to update
  // the view.
  pending_text_ = base::UTF8ToUTF16(label);
}

void GlicButton::ShowDefaultLabel() {
  if (!base::FeatureList::IsEnabled(kGlicButtonHideLabelOnTaskNudge)) {
    return;
  }

  is_animating_text_ = true;
  StartSlidingTextAnimation(/*show=*/true);

  const base::TimeDelta kLabelFadeOutDuration = DurationMs(17);
  const base::TimeDelta kNudgeFadeInStart = DurationMs(50);
  const base::TimeDelta kNudgeFadeInDuration = DurationMs(50);
  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&GlicButton::ApplyTextAndFadeIn,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::make_optional(GetLabelText()),
                              /*delay=*/DurationMs(0), kNudgeFadeInDuration))
      .Once()
      .At(kNudgeFadeInStart - kLabelFadeOutDuration)
      .SetOpacity(label(), 0)
      .SetDuration(kLabelFadeOutDuration);
}

void GlicButton::SuppressLabel() {
  if (!base::FeatureList::IsEnabled(kGlicButtonHideLabelOnTaskNudge)) {
    return;
  }

  is_animating_text_ = true;

  StartSlidingTextAnimation(/*show=*/false);

  label()->SetPaintToLayer();
  label()->layer()->SetFillsBoundsOpaquely(false);
  label()->layer()->SetOpacity(0.0f);
  ApplyTextAndFadeIn(std::make_optional<std::u16string>(u""), DurationMs(0),
                     DurationMs(0));
}

void GlicButton::RestoreDefaultLabel() {
  if (!EntrypointVariationsEnabled()) {
    return SetText(GetLabelText());
  }
  // Store the new label text until the right moment in the animation to update
  // the view.
  pending_text_ = GetLabelText();
}

void GlicButton::SetGlicPanelIsOpen(bool open) {
  glic_panel_is_open_ = open;
  UpdateTextAndBackgroundColors();
}

void GlicButton::OnFreWebUiStateChanged(mojom::FreWebUiState new_state) {
  UpdateTooltipText();
}

void GlicButton::PanelStateChanged(bool active) {
  UpdateTooltipText();
}

void GlicButton::UpdateTooltipText() {
  GlicKeyedService* service =
      GlicKeyedService::Get(tab_strip_controller_->GetProfile());
  // Set tooltip and accessibility text based on whether any glic UI (window or
  // FRE) is open.
  std::u16string tooltip_text = l10n_util::GetStringUTF16(
      service->IsWindowOrFreShowing() ? IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP_CLOSE
                                      : IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP);

  SetTooltipText(tooltip_text);
  GetViewAccessibility().SetName(tooltip_text);
}

void GlicButton::SetIsShowingNudge(bool is_showing) {
  if (is_showing) {
    SetCloseButtonFocusBehavior(FocusBehavior::ALWAYS);
    AnnounceNudgeShown();
    StartShowAnimation();
  } else {
    SetCloseButtonFocusBehavior(FocusBehavior::NEVER);
    StartHideAnimation();
  }

  is_showing_nudge_ = is_showing;
  PreferredSizeChanged();
}

void GlicButton::OnAnimationEnded() {
  if (GetWidthFactor() == 0) {
    RestoreDefaultLabel();
  }
}

gfx::Size GlicButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int current_preferred_width =
      GetLayoutManager()->GetPreferredSize(this, available_size).width();
  const int height =
      TabStripControlButton::CalculatePreferredSize(
          views::SizeBounds(current_preferred_width, available_size.height()))
          .height();

  if (is_animating_text_) {
    const int width = std::lerp(kMinTargetWidthForAnimatingText,
                                default_label_width_, GetWidthFactor());
    return gfx::Size(width, height);
  }

  // Get collapsed and expanded widths, which are set when the show animation
  // starts.
  const int collapsed_width =
      initial_width_ ? initial_width_ : current_preferred_width;
  const int expanded_width =
      expanded_width_ ? expanded_width_ : current_preferred_width;

  // If collapsed width is too small, make it match the height so the button
  // will be square.
  const int collapsed_width_or_square = std::max(collapsed_width, height);

  // Interpolate between collapsed and expanded width based on animation state.
  const int width =
      std::lerp(collapsed_width_or_square, expanded_width, GetWidthFactor());
  return gfx::Size(width, height);
}

void GlicButton::StateChanged(ButtonState old_state) {
  TabStripNudgeButton::StateChanged(old_state);
  if (old_state == STATE_NORMAL && GetState() == STATE_HOVERED) {
    if (hovered_callback_) {
      hovered_callback_.Run();
    }

    MaybeFadeHighlightOnHover(0);
  } else if (old_state == STATE_HOVERED && GetState() == STATE_NORMAL) {
    MaybeFadeHighlightOnHover(1);
  }

  UpdateTextAndBackgroundColors();
  UpdateIcon();
}

void GlicButton::AddedToWidget() {
  if (EntrypointVariationsEnabled()) {
    // Both TabStripControlButton and parent LabelButton set up similar logic
    // here for drawing the button as enabled or disabled when window activation
    // changes. Use LabelButton's as TabStripControlButton fails to update the
    // text color when the window goes from inactive to active.
    // TODO(crbug.com/452116005): Make this behavior configurable on
    // TabStripControlButton.
    LabelButton::AddedToWidget();
  }

  TabStripNudgeButton::AddedToWidget();
  default_label_width_ = GetLayoutManager()->GetPreferredSize(this).width();
}

void GlicButton::SetDropToAttachIndicator(bool indicate) {
  if (indicate) {
    SetBackgroundFrameActiveColorId(ui::kColorSysStateHeaderHover);
  } else {
    SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  }
}

gfx::Rect GlicButton::GetBoundsWithInset() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(GetInsets());
  return bounds;
}

void GlicButton::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (!profile_prefs()->GetBoolean(glic::prefs::kGlicPinnedToTabstrip)) {
    return;
  }

  menu_anchor_higlight_ = AddAnchorHighlight();

  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      menu_model_.get(),
      base::BindRepeating(&GlicButton::OnMenuClosed, base::Unretained(this)));
  menu_model_adapter_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                                   ui::EF_RIGHT_MOUSE_BUTTON);
  std::unique_ptr<views::MenuItemView> root = menu_model_adapter_->CreateMenu();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(root),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void GlicButton::ExecuteCommand(int command_id, int event_flags) {
  CHECK(command_id == IDC_GLIC_TOGGLE_PIN);
  profile_prefs()->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);
}

void GlicButton::SetText(std::u16string_view text) {
  TabStripNudgeButton::SetText(text);
  // Setting label text seems to clear the margin. Set it again.
  label()->SetProperty(views::kMarginsKey,
                       gfx::Insets().set_right(kLabelRightMargin));
}

bool GlicButton::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && mouse_down_callback_) {
    mouse_down_callback_.Run();
    return true;
  }
  return false;
}

void GlicButton::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == expansion_animation_.get()) {
    SetWidthFactor(animation->GetCurrentValue());
  }
}

void GlicButton::AnimationEnded(const gfx::Animation* animation) {
  if (animation == expansion_animation_.get()) {
    AnimationProgressed(animation);

    // If finished hiding, hide the close button so that we're ready to
    // calculate the correct collapsed width when showing next time.
    if (!is_showing_nudge_) {
      SetCloseButtonVisible(false);
    }

    expansion_animation_done_callback_.Run();
  }
  if (is_animating_text_) {
    is_animating_text_ = false;

    // Makes sure the transition of the frames from is_animating_text_ to
    // !is_animating_text_ in CalculatePreferredSize() is smooth.
    initial_width_ = kMinTargetWidthForAnimatingText;
    expanded_width_ = CalculateExpandedWidth();
  }
}

void GlicButton::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

bool GlicButton::IsContextMenuShowingForTest() {
  return menu_runner_ && menu_runner_->IsRunning();
}

std::unique_ptr<ui::SimpleMenuModel> GlicButton::CreateMenuModel() {
  std::unique_ptr<ui::SimpleMenuModel> model =
      std::make_unique<ui::SimpleMenuModel>(this);
  model->AddItemWithStringIdAndIcon(
      IDC_GLIC_TOGGLE_PIN, IDS_GLIC_BUTTON_CXMENU_UNPIN,
      ui::ImageModel::FromVectorIcon(kKeepOffIcon, ui::kColorIcon, 16));
  return model;
}

void GlicButton::OnMenuClosed() {
  menu_anchor_higlight_.reset();
  menu_runner_.reset();
}

void GlicButton::AnnounceNudgeShown() {
  auto announcement = l10n_util::GetStringFUTF16(
      IDS_GLIC_CONTEXTUAL_CUEING_ANNOUNCEMENT,
      GlicLauncherConfiguration::GetGlobalHotkey().GetShortcutText());
  GetViewAccessibility().AnnounceAlert(announcement);
}

void GlicButton::SetDefaultColors() {
  SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
  SetForegroundFrameInactiveColorId(kColorNewTabButtonForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);

  UpdateTextAndBackgroundColors();
}

void GlicButton::UpdateTextAndBackgroundColors() {
  if (!EntrypointVariationsEnabled()) {
    return;
  }

  const bool highlight_visible = IsHighlightVisible();
  if (highlight_visible || ShouldUseAltIcon()) {
    SetBackgroundFrameActiveColorId(ui::kColorSysBase);

    if (highlight_visible) {
      SetForegroundFrameActiveColorId(kTextOnHighlight);
      SetTextColor(STATE_DISABLED, kTextDisabledOnHighlight);
    } else {
      SetForegroundFrameActiveColorId(kForegroundOnAltBackground);
      SetTextColor(STATE_DISABLED, kTextDisabled);
    }
  } else {
    SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
    SetForegroundFrameActiveColorId(kForeground);
    SetTextColor(STATE_DISABLED, kTextDisabled);
  }

  if (base::FeatureList::IsEnabled(features::kGlicButtonPressedState)) {
    if (glic_panel_is_open_) {
      SetBackgroundFrameActiveColorId(kBackgroundWhenGlicOpenActive);
      SetBackgroundFrameInactiveColorId(kBackgroundWhenGlicOpenInactive);
    } else {
      // Active frame background color is set above depending on highlight and
      // icon.
      // TODO(crbug.com/453739403): When GlicButtonPressedState is cleaned up,
      // consolidate the button background logic.
      SetBackgroundFrameInactiveColorId(
          kColorNewTabButtonCRBackgroundFrameInactive);
    }
  }

  UpdateColors();
}

void GlicButton::UpdateIcon() {
  const ui::ImageModel& model =
      IsHighlightVisible() ? icon_for_highlight_ : normal_icon_;

  SetImageModel(views::Button::STATE_NORMAL, model);
  SetImageModel(views::Button::STATE_HOVERED, model);
  SetImageModel(views::Button::STATE_PRESSED, model);
  SetImageModel(views::Button::STATE_DISABLED, model);
}

void GlicButton::MaybeFadeHighlightOnHover(float final_opacity) {
  if (is_showing_nudge_ && HighlightNudgeEnabled()) {
    const base::TimeDelta kFadeDuration = DurationMs(170);
    views::AnimationBuilder()
        .Once()
        .SetOpacity(highlight_view_, final_opacity)
        .SetDuration(kFadeDuration);
  }
}

bool GlicButton::IsHighlightVisible() const {
  return HighlightNudgeEnabled() && is_showing_nudge_ &&
         GetState() != STATE_HOVERED;
}

void GlicButton::StartShowAnimation() {
  if (!EntrypointVariationsEnabled()) {
    // If flag is disabled, the parent drives the animation. Just update the
    // close button.
    return SetCloseButtonVisible(true);
  }

  // Don't restart the animation if already expanding or expanded.
  if (is_showing_nudge_) {
    return;
  }

  // Remember the button's original width before changing the text and showing
  // the close button.
  initial_width_ = GetLayoutManager()->GetPreferredSize(this).width();
  SetCloseButtonVisible(true);
  expanded_width_ = CalculateExpandedWidth();

  const base::TimeDelta kShowDuration = DurationMs(667);
  const base::TimeDelta kCloseButtonFadeStart = DurationMs(333);
  const base::TimeDelta kCloseButtonFadeDuration = DurationMs(333);
  StartExpansionAnimations(
      /*show=*/true, kShowDuration, kCloseButtonFadeStart,
      kCloseButtonFadeDuration);

  const base::TimeDelta kLabelFadeOutDuration = DurationMs(17);
  const base::TimeDelta kNudgeFadeInStart =
      DurationMs(ShouldShowLabel() ? 267 : 150);
  const base::TimeDelta kNudgeFadeInDuration =
      DurationMs(ShouldShowLabel() ? 100 : 200);
  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&GlicButton::ApplyTextAndFadeIn,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(pending_text_),
                              /*delay=*/DurationMs(0), kNudgeFadeInDuration))
      .Once()
      .At(kNudgeFadeInStart - kLabelFadeOutDuration)
      .SetOpacity(label(), 0)
      .SetDuration(kLabelFadeOutDuration);
}

void GlicButton::StartHideAnimation() {
  if (!EntrypointVariationsEnabled()) {
    // If flag is disabled, the parent drives the animation. Just update the
    // close button.
    return SetCloseButtonVisible(false);
  }

  // Don't start the animation if already collapsing or collapsed.
  if (!is_showing_nudge_) {
    return;
  }

  const base::TimeDelta kHideDuration = DurationMs(500);
  const base::TimeDelta kCloseButtonFadeStart = base::TimeDelta();
  const base::TimeDelta kCloseButtonFadeDuration = DurationMs(117);
  StartExpansionAnimations(
      /*show=*/false, kHideDuration, kCloseButtonFadeStart,
      kCloseButtonFadeDuration);

  const base::TimeDelta kNudgeFadeOutStart =
      DurationMs(ShouldShowLabel() ? 0 : 50);
  const base::TimeDelta kNudgeFadeOutDuration =
      DurationMs(ShouldShowLabel() ? 133 : 267);
  const float kNudgeFinalOpacity = ShouldShowLabel() ? 0.5 : 0;
  const base::TimeDelta kLabelFadeInStart = DurationMs(34);
  const base::TimeDelta kLabelFadeInDuration = DurationMs(17);

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&GlicButton::ApplyTextAndFadeIn,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::make_optional(GetLabelText()),
                              kLabelFadeInStart, kLabelFadeInDuration))
      .Once()
      .At(kNudgeFadeOutStart)
      .SetOpacity(label(), kNudgeFinalOpacity)
      .SetDuration(kNudgeFadeOutDuration);
}

void GlicButton::ApplyTextAndFadeIn(std::optional<std::u16string> text,
                                    base::TimeDelta delay,
                                    base::TimeDelta duration) {
  if (text) {
    SetText(*text);
  }

  // This moment coincides with the highlight being midway through its opacity
  // animation. Update text and icon for the final highlight state now.
  UpdateTextAndBackgroundColors();
  UpdateIcon();

  if (is_showing_nudge_) {
    // Start at 50% opacity if replacing default label with nudge.
    label()->layer()->SetOpacity(ShouldShowLabel() ? 0.5 : 0);
  }

  views::AnimationBuilder()
      .Once()
      .At(delay)
      .SetOpacity(label(), 1)
      .SetDuration(duration);
}

int GlicButton::CalculateExpandedWidth() {
  int nudge_text_width = 0;
  // May be unset in tests.
  // TODO(449773402): pending_text_ should always be set here.
  if (pending_text_) {
    // Measure the nudge text.
    auto render_text = gfx::RenderText::CreateRenderText();
    render_text->SetText(*pending_text_);
    render_text->SetFontList(label()->font_list());
    nudge_text_width = render_text->GetStringSize().width();
  }

  const int old_width = GetLayoutManager()->GetPreferredSize(this).width();
  // Replace old label with new.
  int new_width = old_width - label()->width() + nudge_text_width;
  if (!ShouldShowLabel()) {
    // If transitioning from empty label to nudge label, make sure the label
    // margin is included.
    new_width += kLabelRightMargin;
  }
  return new_width;
}

void GlicButton::StartSlidingTextAnimation(bool show) {
  // Button width animation updates width_factor_, used in
  // CalculatePreferredSize().
  if (!expansion_animation_) {
    expansion_animation_ = std::make_unique<gfx::SlideAnimation>(this);
  }

  expansion_animation_->SetTweenType(kSlidingTextTween);

  if (show) {
    expansion_animation_->SetSlideDuration(DurationMs(500));
    expansion_animation_->Show();
  } else {
    expansion_animation_->SetSlideDuration(DurationMs(250));
    expansion_animation_->Hide();
  }
}

void GlicButton::StartExpansionAnimations(
    bool show,
    base::TimeDelta overall_duration,
    base::TimeDelta close_button_fade_start,
    base::TimeDelta close_button_fade_duration) {
  constexpr gfx::Tween::Type kTween = gfx::Tween::EASE_OUT_3;

  // Button width animation updates width_factor_, used in
  // CalculatePreferredSize().
  if (!expansion_animation_) {
    expansion_animation_ = std::make_unique<gfx::SlideAnimation>(this);
    expansion_animation_->SetTweenType(kTween);
  }
  expansion_animation_->SetSlideDuration(overall_duration);
  if (show) {
    // Makes sure the animation value always goes from 0 to 1 for show and 1 to
    // 0 for hide.
    SetWidthFactor(0.f);
    expansion_animation_->Reset(0);
    expansion_animation_->Show();
  } else {
    SetWidthFactor(1.f);
    expansion_animation_->Reset(1);
    expansion_animation_->Hide();
  }

  const float final_highlight_opacity = show && HighlightNudgeEnabled() ? 1 : 0;
  const float final_close_button_opacity = show ? 1 : 0;

  views::AnimationBuilder()
      .Once()
      // Highlight opacity, linear.
      .SetOpacity(highlight_view_, final_highlight_opacity, kTween)
      .SetDuration(overall_duration)
      // Close button opacity, linear.
      .At(close_button_fade_start)
      .SetOpacity(close_button(), final_close_button_opacity)
      .SetDuration(close_button_fade_duration);
}

void GlicButton::CreateIconAndLabelContainer() {
  // Restructure the button to place a "highlight" view behind the icon and
  // label. It's separate from icon_and_label_container so that its opacity can
  // be animated independently.
  //
  // parent (layout: FillLayout)
  // +-> highlight_view_
  // +-> container (layout: horizontal BoxLayout)
  //     +-> image_container_view()
  //     +-> label()

  std::optional<size_t> icon_index = GetIndexOf(image_container_view());
  CHECK(icon_index);
  auto* parent = AddChildViewAt(std::make_unique<views::View>(), *icon_index);
  parent->SetProperty(views::kMarginsKey, gfx::Insets(kHighlightMargin));
  // Don't steal hover events
  parent->SetCanProcessEventsWithinSubtree(false);
  parent->SetLayoutManager(std::make_unique<views::FillLayout>());
  icon_label_highlight_view_ = parent;

  highlight_view_ = parent->AddChildView(std::make_unique<views::View>());
  highlight_view_->SetBackground(views::CreateRoundedRectBackground(
      kHighlightColorId, kHighlightCornerRadius, 0));
  highlight_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  highlight_view_->layer()->SetFillsBoundsOpaquely(false);
  highlight_view_->layer()->SetOpacity(0);

  views::View* icon_and_label_container =
      parent->AddChildView(std::make_unique<views::View>());
  icon_and_label_container->SetPaintToLayer();
  icon_and_label_container->layer()->SetFillsBoundsOpaquely(false);
  auto* const layout_manager = icon_and_label_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>());
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  // Reparent icon and label.
  icon_and_label_container->AddChildView(
      RemoveChildViewT(image_container_view()));
  icon_and_label_container->AddChildView(RemoveChildViewT(label()));
}

void GlicButton::SetCloseButtonVisible(bool visible) {
  close_button()->SetVisible(visible);

  gfx::Insets highlight_margins(kHighlightMargin);
  if (visible) {
    // Nudge text and close button are shown together, and the close button is
    // responsible for all the spacing between them.
    highlight_margins.set_right(0);
  } else if (ShouldShowLabel()) {
    // Close button is hidden. If there's label text, give it extra space.
    highlight_margins.set_right(4);
  }
  icon_label_highlight_view_->SetProperty(views::kMarginsKey,
                                          highlight_margins);

  PreferredSizeChanged();
}

void GlicButton::RefreshBackground() {
  UpdateColors();
}

gfx::SlideAnimation* GlicButton::GetExpansionAnimationForTesting() {
  return expansion_animation_.get();
}

bool GlicButton::GetLabelEnabledForTesting() const {
  return label()->GetEnabled();
}

BEGIN_METADATA(GlicButton)
END_METADATA

}  // namespace glic
