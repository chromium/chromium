// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic/tab_strip_glic_button.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/glic/glic_actor_constants.h"
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

namespace {

const base::FeatureParam<bool> kAdjustMargins{
    &features::kGlicButtonAltLabel, "glic-button-alt-label-adjust-margins",
    true};

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

constexpr int kIconSize = 16;
constexpr int kCollapsedWidth = 41;

bool EntrypointVariationsEnabled() {
  return base::FeatureList::IsEnabled(features::kGlicEntrypointVariations);
}

bool ShouldShowLabel() {
  return EntrypointVariationsEnabled() &&
         features::kGlicEntrypointVariationsShowLabel.Get();
}

std::u16string GetLabelText() {
  if (!ShouldShowLabel()) {
    return std::u16string();
  }

  if (base::FeatureList::IsEnabled(features::kGlicButtonAltLabel)) {
    switch (features::kGlicButtonAltLabelVariant.Get()) {
      case 0:
        return l10n_util::GetStringUTF16(
            IDS_GLIC_BUTTON_ENTRYPOINT_ASK_GEMINI_LABEL);
      case 1:
        return l10n_util::GetStringUTF16(
            IDS_GLIC_BUTTON_ENTRYPOINT_ASK_BROWSER_LABEL);
      case 2:
        return l10n_util::GetStringUTF16(
            IDS_GLIC_BUTTON_ENTRYPOINT_BROWSE_LABEL);
      default:
        break;
    }
  }
  return l10n_util::GetStringUTF16(IDS_GLIC_BUTTON_ENTRYPOINT_LABEL);
}

bool ShouldUseAltIcon() {
  // LINT.IfChange(ShouldUseAltIcon)
  return EntrypointVariationsEnabled() &&
         features::kGlicEntrypointVariationsAltIcon.Get();
  // LINT.ThenChange(//chrome/browser/ui/views/tabs/glic/glic_actor_task_icon.cc:ShouldUseGlicButtonAltIconBackgroundColor)
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
  return ui::ImageModel::FromVectorIcon(GlicVectorIcon(), kForeground,
                                        kIconSize);
}

ui::ImageModel GetIconForHighlight() {
  return ui::ImageModel::FromVectorIcon(
      GlicVectorIcon(),
      HighlightNudgeEnabled() ? kTextOnHighlight : kForeground, kIconSize);
}

gfx::Insets GetIconMargins(bool label_shown) {
  int left = 6 - kHighlightMargin;
  int right = 4;

  if (label_shown) {
    // Extra left margin if the label is shown.
    left += 2;
  }

  if (base::FeatureList::IsEnabled(features::kGlicButtonAltLabel) &&
      kAdjustMargins.Get()) {
    // TODO(crbug.com/485624752): Consolidate after launch.
    right += 1;
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

class TabStripGlicButton::WidthAnimationController
    : public gfx::AnimationDelegate {
 public:
  WidthAnimationController(TabStripGlicButton& button,
                           base::RepeatingClosure animation_done_callback)
      : button_(button),
        animation_done_callback_(std::move(animation_done_callback)) {}

  void Start(WidthState old_state, WidthState new_state) {
    DCHECK(old_state != new_state);

    const bool to_or_from_nudge =
        old_state == WidthState::kNudge || new_state == WidthState::kNudge;

    const base::TimeDelta duration = GetDuration(old_state, new_state);

    animation_.SetTweenType(to_or_from_nudge ? kNudgeTween : kCollapseTween);
    animation_.SetSlideDuration(duration);
    button_->SetWidthFactor(0.f);
    animation_.Reset(0);
    animation_.Show();

    if (to_or_from_nudge) {
      StartOpacityAnimationsForNudge(duration, new_state);
    }
  }

  gfx::SlideAnimation* GetAnimationForTesting() { return &animation_; }

 private:
  static constexpr gfx::Tween::Type kNudgeTween = gfx::Tween::EASE_OUT_3;
  // TODO(crbug.com/460400955): Move this constant to a shared location.
  // This should mirror the tween used for TabStripNudgeAnimationSession.
  static constexpr gfx::Tween::Type kCollapseTween =
      gfx::Tween::ACCEL_20_DECEL_100;

  static base::TimeDelta GetDuration(WidthState old_state,
                                     WidthState new_state) {
    if (old_state == WidthState::kNormal) {
      if (new_state == WidthState::kNudge) {
        return DurationMs(667);
      }
      return DurationMs(250);
    }
    return DurationMs(500);
  }

  void StartOpacityAnimationsForNudge(base::TimeDelta duration,
                                      WidthState new_state) {
    const bool show = (new_state == WidthState::kNudge);
    const float final_highlight_opacity =
        show && HighlightNudgeEnabled() ? 1 : 0;
    const float final_close_button_opacity = show ? 1 : 0;
    const base::TimeDelta close_button_fade_start =
        show ? DurationMs(333) : base::TimeDelta();
    const base::TimeDelta close_button_fade_duration =
        show ? DurationMs(333) : DurationMs(117);

    views::AnimationBuilder()
        .Once()
        // Highlight opacity, linear.
        .SetOpacity(button_->highlight_view(), final_highlight_opacity,
                    kNudgeTween)
        .SetDuration(duration)
        // Close button opacity, linear.
        .At(close_button_fade_start)
        .SetOpacity(button_->close_button(), final_close_button_opacity)
        .SetDuration(close_button_fade_duration);
  }

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    button_->SetWidthFactor(animation->GetCurrentValue());
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    AnimationProgressed(animation);

    button_->OnAnimationEnded();
    animation_done_callback_.Run();
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  raw_ref<TabStripGlicButton> button_;
  base::RepeatingClosure animation_done_callback_;
  gfx::SlideAnimation animation_{this};
};

TabStripGlicButton::TabStripGlicButton(
    BrowserWindowInterface* browser_window_interface,
    PressedCallback pressed_callback,
    PressedCallback close_pressed_callback,
    base::RepeatingClosure hovered_callback,
    base::RepeatingClosure mouse_down_callback,
    base::RepeatingClosure expansion_animation_done_callback,
    const std::u16string& tooltip)
    : TabStripNudgeButton(browser_window_interface,
                          std::move(pressed_callback),
                          std::move(close_pressed_callback),
                          GetLabelText(),
                          kGlicNudgeButtonElementId,
                          Edge::kNone,
                          gfx::VectorIcon::EmptyIcon(),
                          /*show_close_button=*/true),
      menu_model_(CreateMenuModel()),
      browser_window_interface_(browser_window_interface),
      profile_(browser_window_interface ? browser_window_interface->GetProfile()
                                        : nullptr),
      hovered_callback_(std::move(hovered_callback)),
      mouse_down_callback_(std::move(mouse_down_callback)),
      normal_icon_(GetNormalIcon()),
      icon_for_highlight_(GetIconForHighlight()),
      width_animation_controller_(
          std::make_unique<TabStripGlicButton::WidthAnimationController>(
              *this,
              std::move(expansion_animation_done_callback))) {
  SetProperty(views::kElementIdentifierKey, kGlicButtonElementId);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  UpdateIcon();
  OnLabelVisibilityChanged();
  auto* image_view = static_cast<views::ImageView*>(image_container_view());
  image_view->SetImageSize({kIconSize, kIconSize});
  image_view->SetPaintToLayer();
  image_view->layer()->SetFillsBoundsOpaquely(false);

  CreateIconAndLabelContainer();

  if (!label()->layer()) {
    // Make sure label() has a layer even if its text is empty, so we can use
    // the same opacity animation whether or not the label has text.
    label()->SetPaintToLayer();
  }
  SetLabelMargins();
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
}

TabStripGlicButton::~TabStripGlicButton() = default;

void TabStripGlicButton::SetNudgeLabel(std::string label) {
  if (!EntrypointVariationsEnabled()) {
    start_width_ = PreferredSize().width();
    return SetText(base::UTF8ToUTF16(label));
  }
  // Store the new label text until the right moment in the animation to update
  // the view.
  pending_text_ = base::UTF8ToUTF16(label);
}

void TabStripGlicButton::Expand() {
  // Update state.
  if (width_state_ != WidthState::kCollapsed) {
    return;
  }
  WidthState old_width_state = width_state_;
  SetWidthState(WidthState::kNormal);

  // If the label should not show, no further animation is needed.
  if (!ShouldShowLabel()) {
    return;
  }

  start_width_ = kCollapsedWidth;
  end_width_ = normal_width_;
  width_animation_controller_->Start(old_width_state, width_state_);

  const base::TimeDelta kLabelFadeOutDuration = DurationMs(17);
  const base::TimeDelta kNudgeFadeInStart = DurationMs(50);
  const base::TimeDelta kNudgeFadeInDuration = DurationMs(50);
  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&TabStripGlicButton::ApplyTextAndFadeIn,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::make_optional(GetLabelText()),
                              /*delay=*/DurationMs(0), kNudgeFadeInDuration))
      .Once()
      .At(kNudgeFadeInStart - kLabelFadeOutDuration)
      .SetOpacity(label(), 0)
      .SetDuration(kLabelFadeOutDuration);
}

void TabStripGlicButton::Collapse() {
  WidthState old_width_state = width_state_;
  if (width_state_ == WidthState::kCollapsed) {
    return;
  }
  SetWidthState(WidthState::kCollapsed);

  start_width_ = PreferredSize().width();
  end_width_ = kCollapsedWidth;
  width_animation_controller_->Start(old_width_state, width_state_);

  label()->SetPaintToLayer();
  label()->layer()->SetFillsBoundsOpaquely(false);
  label()->layer()->SetOpacity(0.0f);
  ApplyTextAndFadeIn(std::make_optional<std::u16string>(u""), DurationMs(0),
                     DurationMs(0));
}

void TabStripGlicButton::RestoreDefaultLabel() {
  if (!EntrypointVariationsEnabled()) {
    return SetText(GetLabelText());
  }
  // Store the new label text until the right moment in the animation to update
  // the view.
  pending_text_ = GetLabelText();
}

void TabStripGlicButton::SetGlicPanelIsOpen(bool open) {
  if (glic_panel_is_open_ == open) {
    return;
  }

  glic_panel_is_open_ = open;
  UpdateTextAndBackgroundColors();
  UpdateIcon();

  // The tooltip reflects whether clicking will open or close glic.
  std::u16string tooltip_text =
      l10n_util::GetStringUTF16(open ? IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP_CLOSE
                                     : IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP);
  SetTooltipText(tooltip_text);

  // The accessibility text mirrors the visible label, unless glic is open, in
  // which case this text should communicate that clicking will close it.
  GetViewAccessibility().SetName(
      open ? l10n_util::GetStringUTF16(IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP_CLOSE)
           : GetLabelText());
}

void TabStripGlicButton::SetIsShowingNudge(bool is_showing) {
  if (is_showing) {
    SetCloseButtonFocusBehavior(FocusBehavior::ALWAYS);
    AnnounceNudgeShown();
    ShowNudge();
  } else {
    SetCloseButtonFocusBehavior(FocusBehavior::NEVER);
    HideNudge();
  }

  PreferredSizeChanged();
}

bool TabStripGlicButton::GetIsShowingNudge() const {
  return width_state_ == WidthState::kNudge;
}

void TabStripGlicButton::OnAnimationEnded() {
  // TODO(crbug.com/469850069): Remove.
  if (!EntrypointVariationsEnabled()) {
    if (GetWidthFactor() == 0) {
      RestoreDefaultLabel();
    }
    return;
  }

  if (IsHidingNudge()) {
    SetCloseButtonVisible(false);
  }

  last_width_state_ = width_state_;
  OnLabelVisibilityChanged();
}

gfx::Size TabStripGlicButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int current_preferred_width =
      GetLayoutManager()->GetPreferredSize(this, available_size).width();
  const int height =
      TabStripControlButton::CalculatePreferredSize(
          views::SizeBounds(current_preferred_width, available_size.height()))
          .height();

  // Button must always be at least as wide as it is tall.
  int start = std::max(start_width_, height);
  int end = std::max(end_width_, height);

  // TODO(crbug.com/469850069): Remove.
  if (!EntrypointVariationsEnabled()) {
    start = kCollapsedWidth;
    end = current_preferred_width;
  }

  // Interpolate based on animation value.
  const int width = std::lerp(start, end, GetWidthFactor());
  return gfx::Size(width, height);
}

void TabStripGlicButton::StateChanged(ButtonState old_state) {
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

void TabStripGlicButton::AddedToWidget() {
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
  // Button starts in WidthState::kNormal. Measure that state's width and set
  // `start_width_` and `end_width_` for CalculatePreferredSize().
  normal_width_ = PreferredSize().width();
  start_width_ = normal_width_;
  end_width_ = normal_width_;

  window_did_become_active_subscription_ =
      browser_window_interface_->RegisterDidBecomeActive(base::BindRepeating(
          &TabStripGlicButton::OnBrowserWindowDidBecomeActive,
          base::Unretained(this)));
  window_did_become_inactive_subscription_ =
      browser_window_interface_->RegisterDidBecomeInactive(base::BindRepeating(
          &TabStripGlicButton::OnBrowserWindowDidBecomeInactive,
          base::Unretained(this)));

  UpdateInkdropHoverColor(browser_window_interface_->IsActive());
}

void TabStripGlicButton::SetDropToAttachIndicator(bool indicate) {
  if (indicate) {
    SetBackgroundFrameActiveColorId(ui::kColorSysStateHeaderHover);
  } else {
    SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  }
}

gfx::Rect TabStripGlicButton::GetBoundsWithInset() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(GetInsets());
  return bounds;
}

void TabStripGlicButton::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (!GetPrefService()->GetBoolean(glic::prefs::kGlicPinnedToTabstrip)) {
    return;
  }

  menu_anchor_higlight_ = AddAnchorHighlight();

  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      menu_model_.get(), base::BindRepeating(&TabStripGlicButton::OnMenuClosed,
                                             base::Unretained(this)));
  menu_model_adapter_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                                   ui::EF_RIGHT_MOUSE_BUTTON);
  std::unique_ptr<views::MenuItemView> root = menu_model_adapter_->CreateMenu();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(root),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  menu_runner_->RunMenuAt(GetWidget(), nullptr, GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void TabStripGlicButton::ExecuteCommand(int command_id, int event_flags) {
  CHECK(command_id == IDC_GLIC_TOGGLE_PIN);
  GetPrefService()->SetBoolean(glic::prefs::kGlicPinnedToTabstrip, false);
}

void TabStripGlicButton::SetText(std::u16string_view text) {
  TabStripNudgeButton::SetText(text);
  // Setting label text seems to clear the margin. Set it again.
  SetLabelMargins();
}

bool TabStripGlicButton::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && mouse_down_callback_) {
    mouse_down_callback_.Run();
    return true;
  }
  return false;
}

bool TabStripGlicButton::IsContextMenuShowingForTest() {
  return menu_runner_ && menu_runner_->IsRunning();
}

std::unique_ptr<ui::SimpleMenuModel> TabStripGlicButton::CreateMenuModel() {
  std::unique_ptr<ui::SimpleMenuModel> model =
      std::make_unique<ui::SimpleMenuModel>(this);
  model->AddItemWithStringIdAndIcon(
      IDC_GLIC_TOGGLE_PIN, IDS_GLIC_BUTTON_CXMENU_UNPIN,
      ui::ImageModel::FromVectorIcon(kKeepOffIcon, ui::kColorIcon, 16));
  return model;
}

void TabStripGlicButton::OnMenuClosed() {
  menu_anchor_higlight_.reset();
  menu_runner_.reset();
}

void TabStripGlicButton::AnnounceNudgeShown() {
  auto announcement = l10n_util::GetStringFUTF16(
      IDS_GLIC_CONTEXTUAL_CUEING_ANNOUNCEMENT,
      GlicLauncherConfiguration::GetGlobalHotkey().GetShortcutText());
  GetViewAccessibility().AnnounceAlert(announcement);
}

PrefService* TabStripGlicButton::GetPrefService() {
  return profile_->GetPrefs();
}

void TabStripGlicButton::SetDefaultColors() {
  SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
  SetForegroundFrameInactiveColorId(kColorNewTabButtonForegroundFrameInactive);
  SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorNewTabButtonCRBackgroundFrameInactive);

  UpdateTextAndBackgroundColors();
}

void TabStripGlicButton::UpdateTextAndBackgroundColors() {
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

  if (base::FeatureList::IsEnabled(features::kGlicButtonPressedState) &&
      GetWidget()) {
    SetHighlighted(glic_panel_is_open_);
  }

  UpdateColors();
}

void TabStripGlicButton::NotifyClick(const ui::Event& event) {
  if (base::FeatureList::IsEnabled(features::kGlicButtonPressedState)) {
    // TabStripControlButton manipulates the ink drop in its NotifyClick(), so
    // if we're using the ink drop to show the button's pressed state, skip
    // TabStripControlButton::NotifyClick() and just call the base
    // NotifyClick().
    LabelButton::NotifyClick(event);
  } else {
    TabStripNudgeButton::NotifyClick(event);
  }
}

void TabStripGlicButton::UpdateIcon() {
  const bool solid_icon_for_pressed_state =
      base::FeatureList::IsEnabled(features::kGlicButtonPressedState) &&
      features::kGlicButtonPressedForceSolidIcon.Get() && glic_panel_is_open_;
  const ui::ImageModel& model =
      (solid_icon_for_pressed_state || IsHighlightVisible())
          ? icon_for_highlight_
          : normal_icon_;

  SetImageModel(views::Button::STATE_NORMAL, model);
  SetImageModel(views::Button::STATE_HOVERED, model);
  SetImageModel(views::Button::STATE_PRESSED, model);
  SetImageModel(views::Button::STATE_DISABLED, model);
}

void TabStripGlicButton::MaybeFadeHighlightOnHover(float final_opacity) {
  if (GetIsShowingNudge() && HighlightNudgeEnabled()) {
    const base::TimeDelta kFadeDuration = DurationMs(170);
    views::AnimationBuilder()
        .Once()
        .SetOpacity(highlight_view_, final_opacity)
        .SetDuration(kFadeDuration);
  }
}

bool TabStripGlicButton::IsHighlightVisible() const {
  return HighlightNudgeEnabled() && GetIsShowingNudge() &&
         GetState() != STATE_HOVERED;
}

void TabStripGlicButton::ShowNudge() {
  WidthState old_width_state = width_state_;
  collapsed_before_nudge_shown_ = width_state_ == WidthState::kCollapsed;
  // Don't restart the animation if already nudging.
  if (width_state_ == WidthState::kNudge) {
    return;
  }
  SetWidthState(WidthState::kNudge);

  if (!EntrypointVariationsEnabled()) {
    // If flag is disabled, the parent drives the animation. Just update the
    // close button.
    return SetCloseButtonVisible(true);
  }

  // Remember the button's original width before changing the text and showing
  // the close button.
  start_width_ = PreferredSize().width();
  SetCloseButtonVisible(true);
  end_width_ = CalculateExpandedWidth();

  width_animation_controller_->Start(old_width_state, width_state_);

  const base::TimeDelta kLabelFadeOutDuration = DurationMs(17);
  const base::TimeDelta kNudgeFadeInStart =
      DurationMs(ShouldShowLabel() ? 267 : 150);
  const base::TimeDelta kNudgeFadeInDuration =
      DurationMs(ShouldShowLabel() ? 100 : 200);
  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&TabStripGlicButton::ApplyTextAndFadeIn,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::move(pending_text_),
                              /*delay=*/DurationMs(0), kNudgeFadeInDuration))
      .Once()
      .At(kNudgeFadeInStart - kLabelFadeOutDuration)
      .SetOpacity(label(), 0)
      .SetDuration(kLabelFadeOutDuration);
}

void TabStripGlicButton::HideNudge() {
  WidthState old_width_state = width_state_;
  // Only animate if transitioning from kNudge to kNormal or kCollapsed.
  if (width_state_ != WidthState::kNudge) {
    return;
  }
  // If the label was previously collapsed, return to the collapsed state.
  if (collapsed_before_nudge_shown_) {
    Collapse();
    return;
  }
  // If the button wasn't collapsed, it must be transitioning back to kNormal.
  SetWidthState(WidthState::kNormal);

  if (!EntrypointVariationsEnabled()) {
    // If flag is disabled, the parent drives the animation. Just update the
    // close button.
    return SetCloseButtonVisible(false);
  }

  start_width_ = PreferredSize().width();
  end_width_ = normal_width_;
  width_animation_controller_->Start(old_width_state, width_state_);

  const base::TimeDelta kNudgeFadeOutStart =
      DurationMs(ShouldShowLabel() ? 0 : 50);
  const base::TimeDelta kNudgeFadeOutDuration =
      DurationMs(ShouldShowLabel() ? 133 : 267);
  const float kNudgeFinalOpacity = ShouldShowLabel() ? 0.5 : 0;
  const base::TimeDelta kLabelFadeInStart = DurationMs(34);
  const base::TimeDelta kLabelFadeInDuration = DurationMs(17);

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&TabStripGlicButton::ApplyTextAndFadeIn,
                              weak_ptr_factory_.GetWeakPtr(),
                              std::make_optional(GetLabelText()),
                              kLabelFadeInStart, kLabelFadeInDuration))
      .Once()
      .At(kNudgeFadeOutStart)
      .SetOpacity(label(), kNudgeFinalOpacity)
      .SetDuration(kNudgeFadeOutDuration);
}

void TabStripGlicButton::ApplyTextAndFadeIn(std::optional<std::u16string> text,
                                            base::TimeDelta delay,
                                            base::TimeDelta duration) {
  if (text) {
    SetText(*text);
  }

  // This moment coincides with the highlight being midway through its opacity
  // animation. Update text and icon for the final highlight state now.
  UpdateTextAndBackgroundColors();
  UpdateIcon();

  if (width_state_ == WidthState::kNudge) {
    // Start at 50% opacity if replacing default label with nudge.
    label()->layer()->SetOpacity(ShouldShowLabel() ? 0.5 : 0);
  }

  views::AnimationBuilder()
      .Once()
      .At(delay)
      .SetOpacity(label(), 1)
      .SetDuration(duration);
}

int TabStripGlicButton::CalculateExpandedWidth() {
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

  const int old_width = PreferredSize().width();
  // Replace old label with new.
  int new_width = old_width - label()->width() + nudge_text_width;
  if (!ShouldShowLabel()) {
    // If transitioning from empty label to nudge label, make sure the label
    // margin is included.
    new_width += kLabelRightMargin;
  }
  if (last_width_state_ == WidthState::kCollapsed) {
    // Add extra margin if the label was previously collapsed, as the old_width
    // is smaller.
    new_width += kCloseButtonMargin;
  }
  return new_width;
}

void TabStripGlicButton::CreateIconAndLabelContainer() {
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

void TabStripGlicButton::SetCloseButtonVisible(bool visible) {
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

void TabStripGlicButton::RefreshBackground() {
  UpdateColors();
}

void TabStripGlicButton::OnLabelVisibilityChanged() {
  image_container_view()->SetProperty(
      views::kMarginsKey,
      GetIconMargins(ShouldShowLabel() && !IsAnimatingTextVisibility()));
}

bool TabStripGlicButton::IsAnimatingTextVisibility() const {
  return width_state_ == WidthState::kCollapsed ||
         last_width_state_ == WidthState::kCollapsed;
}

bool TabStripGlicButton::IsHidingNudge() const {
  return (width_state_ == WidthState::kNormal ||
          width_state_ == WidthState::kCollapsed) &&
         last_width_state_ == WidthState::kNudge;
}

void TabStripGlicButton::SetWidthState(WidthState state) {
  last_width_state_ = width_state_;
  width_state_ = state;
}

gfx::Size TabStripGlicButton::PreferredSize() const {
  return GetLayoutManager()->GetPreferredSize(this);
}

gfx::SlideAnimation* TabStripGlicButton::GetExpansionAnimationForTesting() {
  return width_animation_controller_->GetAnimationForTesting();  // IN-TEST
}

bool TabStripGlicButton::GetLabelEnabledForTesting() const {
  return label()->GetEnabled();
}

void TabStripGlicButton::SetSplitButtonCornerStyling() {
  SetLeftRightCornerRadii(kSplitButtonRoundedEdgeRadius,
                          kSplitButtonFlatEdgeRadius);
}

void TabStripGlicButton::ResetSplitButtonCornerStyling() {
  SetLeftRightCornerRadii(TabStripNudgeButton::GetCornerRadius(),
                          TabStripNudgeButton::GetCornerRadius());
}

void TabStripGlicButton::RemovedFromWidget() {
  window_did_become_active_subscription_ = {};
  window_did_become_inactive_subscription_ = {};
  TabStripNudgeButton::RemovedFromWidget();
}

void TabStripGlicButton::OnBrowserWindowDidBecomeActive(
    BrowserWindowInterface* bwi) {
  UpdateInkdropHoverColor(true);
}

void TabStripGlicButton::OnBrowserWindowDidBecomeInactive(
    BrowserWindowInterface* bwi) {
  UpdateInkdropHoverColor(false);
}

void TabStripGlicButton::UpdateInkdropHoverColor(bool is_frame_active) {
  SetInkdropHoverColorId(is_frame_active
                             ? kColorTabBackgroundInactiveHoverFrameActive
                             : kColorTabBackgroundInactiveHoverFrameInactive);
  UpdateColors();
}

void TabStripGlicButton::SetLabelMargins() {
  int bottom = 0;
  if (base::FeatureList::IsEnabled(features::kGlicButtonAltLabel) &&
      kAdjustMargins.Get()) {
    bottom += 1;
  }
  label()->SetProperty(
      views::kMarginsKey,
      gfx::Insets().set_right(kLabelRightMargin).set_bottom(bottom));
}

BEGIN_METADATA(TabStripGlicButton)
END_METADATA

}  // namespace glic
