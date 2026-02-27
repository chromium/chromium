// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_H_

#include <concepts>
#include <memory>
#include <string>
#include <variant>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/glic/glic_base_shim.h"
#include "chrome/browser/ui/views/glic/glic_button_interface.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_variant.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

class BrowserWindowInterface;
class Profile;
class TabStripNudgeButton;
class ToolbarButton;

namespace views {
class LabelButton;
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

namespace glic {
inline constexpr int kHighlightMargin = 2;
inline constexpr int kHighlightCornerRadius = 8;
inline constexpr int kCloseButtonMargin = 6;
inline constexpr int kLabelRightMargin = 8;
inline constexpr ui::ColorId kTextOnHighlight = ui::kColorSysOnPrimary;
inline constexpr ui::ColorId kTextDisabledOnHighlight = kTextOnHighlight;
inline constexpr ui::ColorId kTextDisabled = ui::kColorLabelForegroundDisabled;

inline constexpr ui::ColorId kForeground =
    kColorNewTabButtonForegroundFrameActive;
inline constexpr ui::ColorId kForegroundOnAltBackground =
    ui::kColorSysOnSurface;
inline constexpr ui::ColorId kHighlightColorId = ui::kColorSysPrimary;

inline constexpr int kCollapsedWidth = 41;
inline constexpr int kSplitButtonFlatEdgeRadius = 2;

template <typename T>
  requires std::derived_from<T, views::LabelButton>
class GlicButton : public GlicBaseShim<T>,
                   public ui::SimpleMenuModel::Delegate {
 public:
  // These states represent the button's width and label contents.
  enum class WidthState {
    // Spark icon and "Gemini".
    kNormal,

    // Spark icon, contextual nudge text and "X" close button.
    kNudge,

    // Just the spark icon.
    kCollapsed
  };
  class WidthAnimationController : public gfx::AnimationDelegate {
   public:
    WidthAnimationController(GlicButton<T>& button,
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
      if (button_->close_button() == nullptr) {
        return;
      }
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

    raw_ref<GlicButton<T>> button_;
    base::RepeatingClosure animation_done_callback_;
    gfx::SlideAnimation animation_{this};
  };

  template <typename... BaseArgs>
  explicit GlicButton(BrowserWindowInterface* browser_window_interface,
                      base::RepeatingClosure hovered_callback,
                      base::RepeatingClosure mouse_down_callback,
                      base::RepeatingClosure expansion_animation_done_callback,
                      const std::u16string& tooltip,
                      BaseArgs&&... base_args)
      : GlicBaseShim<T>(std::move(base_args)...),
        browser_window_interface_(browser_window_interface),
        menu_model_(CreateMenuModel()),
        profile_(browser_window_interface
                     ? browser_window_interface->GetProfile()
                     : nullptr),
        hovered_callback_(std::move(hovered_callback)),
        mouse_down_callback_(std::move(mouse_down_callback)),
        normal_icon_(GetNormalIcon()),
        icon_for_highlight_(GetIconForHighlight()) {
    Init(expansion_animation_done_callback, tooltip);
  }

  GlicButton(const GlicButton&) = delete;
  GlicButton& operator=(const GlicButton&) = delete;
  ~GlicButton() override = default;

  // These functions below work together to hide the nudge label on the static
  // button when another nudge occupies the display space.
  //
  // Suppresses the default label on the glic button with a hide animation.
  void Collapse() {
    WidthState old_width_state = width_state_;
    if (width_state_ == WidthState::kCollapsed) {
      return;
    }
    SetWidthState(WidthState::kCollapsed);

    start_width_ = PreferredSize().width();
    end_width_ = kCollapsedWidth;
    width_animation_controller_->Start(old_width_state, width_state_);

    this->label()->SetPaintToLayer();
    this->label()->layer()->SetFillsBoundsOpaquely(false);
    this->label()->layer()->SetOpacity(0.0f);
    ApplyTextAndFadeIn(std::make_optional<std::u16string>(u""), DurationMs(0),
                       DurationMs(0));
  }

  // Shows the default label on the glic button with a show animation.
  void Expand() {
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
        .OnEnded(base::BindOnce(&GlicButton::ApplyTextAndFadeIn,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::make_optional(GetLabelText()),
                                /*delay=*/DurationMs(0), kNudgeFadeInDuration))
        .Once()
        .At(kNudgeFadeInStart - kLabelFadeOutDuration)
        .SetOpacity(this->label(), 0)
        .SetDuration(kLabelFadeOutDuration);
  }

  void SetNudgeLabel(std::string label) {
    if (!EntrypointVariationsEnabled()) {
      start_width_ = PreferredSize().width();
      return SetText(base::UTF8ToUTF16(label));
    }
    // Store the new label text until the right moment in the animation to
    // update the view.
    pending_text_ = base::UTF8ToUTF16(label);
  }

  void RestoreDefaultLabel() {
    if (!EntrypointVariationsEnabled()) {
      return SetText(GetLabelText());
    }
    // Store the new label text until the right moment in the animation to
    // update the view.
    pending_text_ = GetLabelText();
  }

  void SetGlicPanelIsOpen(bool open) {
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
    this->SetTooltipText(tooltip_text);

    // The accessibility text mirrors the visible label, unless glic is open, in
    // which case this text should communicate that clicking will close it.
    this->GetViewAccessibility().SetName(
        open
            ? l10n_util::GetStringUTF16(IDS_GLIC_TAB_STRIP_BUTTON_TOOLTIP_CLOSE)
            : GetLabelText());
  }

  void SetIsShowingNudge(bool is_showing) override {
    if (is_showing) {
      SetCloseButtonFocusBehavior(views::View::FocusBehavior::ALWAYS);
      AnnounceNudgeShown();
      ShowNudge();
    } else {
      SetCloseButtonFocusBehavior(views::View::FocusBehavior::NEVER);
      HideNudge();
    }

    this->PreferredSizeChanged();
  }

  bool GetIsShowingNudge() const { return width_state_ == WidthState::kNudge; }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const int current_preferred_width =
        this->GetLayoutManager()
            ->GetPreferredSize(this, available_size)
            .width();

    const int height =
        T::CalculatePreferredSize(
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

  void StateChanged(views::Button::ButtonState old_state) override {
    T::StateChanged(old_state);
    if (old_state == views::Button::ButtonState::STATE_NORMAL &&
        this->GetState() == views::Button::ButtonState::STATE_HOVERED) {
      if (hovered_callback_) {
        hovered_callback_.Run();
      }

      MaybeFadeHighlightOnHover(0);
    } else if (old_state == views::Button::ButtonState::STATE_HOVERED &&
               this->GetState() == views::Button::ButtonState::STATE_NORMAL) {
      MaybeFadeHighlightOnHover(1);
    }

    UpdateTextAndBackgroundColors();
    UpdateIcon();
  }

  void AddedToWidget() override {
    if (EntrypointVariationsEnabled()) {
      // Both TabStripControlButton and parent LabelButton set up similar logic
      // here for drawing the button as enabled or disabled when window
      // activation changes. Use LabelButton's as TabStripControlButton fails to
      // update the text color when the window goes from inactive to active.
      // TODO(crbug.com/452116005): Make this behavior configurable on
      // TabStripControlButton.
      views::LabelButton::AddedToWidget();
    }

    T::AddedToWidget();
    // Button starts in WidthState::kNormal. Measure that state's width and set
    // `start_width_` and `end_width_` for CalculatePreferredSize().
    normal_width_ = PreferredSize().width();
    start_width_ = normal_width_;
    end_width_ = normal_width_;

    window_did_become_active_subscription_ =
        browser_window_interface_->RegisterDidBecomeActive(
            base::BindRepeating(&GlicButton::OnBrowserWindowDidBecomeActive,
                                base::Unretained(this)));
    window_did_become_inactive_subscription_ =
        browser_window_interface_->RegisterDidBecomeInactive(
            base::BindRepeating(&GlicButton::OnBrowserWindowDidBecomeInactive,
                                base::Unretained(this)));

    UpdateInkdropHoverColor(browser_window_interface_->IsActive());
  }

  void RemovedFromWidget() override {
    window_did_become_active_subscription_ = {};
    window_did_become_inactive_subscription_ = {};
    T::RemovedFromWidget();
  }

  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override {
    if (!GetPrefService()->GetBoolean(glic::prefs::kGlicPinnedToTabstrip)) {
      return;
    }

    menu_anchor_higlight_ = this->AddAnchorHighlight();

    menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
        menu_model_.get(), base::BindRepeating(&GlicButton<T>::OnMenuClosed,
                                               base::Unretained(this)));
    menu_model_adapter_->set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                                     ui::EF_RIGHT_MOUSE_BUTTON);
    std::unique_ptr<views::MenuItemView> root =
        menu_model_adapter_->CreateMenu();
    menu_runner_ = std::make_unique<views::MenuRunner>(
        std::move(root),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
    menu_runner_->RunMenuAt(this->GetWidget(), nullptr,
                            this->GetAnchorBoundsInScreen(),
                            views::MenuAnchorPosition::kTopLeft, source_type);
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    CHECK(command_id == IDC_GLIC_TOGGLE_PIN);
    this->GetPrefService()->SetBoolean(glic::prefs::kGlicPinnedToTabstrip,
                                       false);
  }

  // views::View:
  // Note that this is an optimization for fetching zero-state suggestions so
  // that we can load the suggestions in the UI as quickly as possible.
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton() && mouse_down_callback_) {
      mouse_down_callback_.Run();
      return true;
    }
    return false;
  }

  bool IsContextMenuShowingForTest() {
    return menu_runner_ && menu_runner_->IsRunning();
  }

  // Sets the button back to its default colors.
  void SetDefaultColors() {
    SetForegroundFrameActiveColorId(kColorNewTabButtonForegroundFrameActive);
    SetForegroundFrameInactiveColorId(
        kColorNewTabButtonForegroundFrameInactive);
    SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
    SetBackgroundFrameInactiveColorId(
        kColorNewTabButtonCRBackgroundFrameInactive);

    UpdateTextAndBackgroundColors();
  }

  // Called when the slide animation finishes.
  void OnAnimationEnded() {
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

  // virtual gfx::SlideAnimation* GetExpansionAnimationForTesting();
  bool GetLabelEnabledForTesting() const { return this->label()->GetEnabled(); }

  // Updates the background painter to match the current border insets.
  void RefreshBackground() { UpdateColors(); }

  // Show or hide the split button styling, used when the task indicator is
  // present.
  void SetSplitButtonCornerStyling() {
    SetLeftRightCornerRadii(kSplitButtonFlatEdgeRadius,
                            kSplitButtonFlatEdgeRadius);
  }

  void ResetSplitButtonCornerStyling() {
    SetLeftRightCornerRadii(T::GetCornerRadius(), T::GetCornerRadius());
  }

  void OnBrowserWindowDidBecomeActive(BrowserWindowInterface* bwi) {
    UpdateInkdropHoverColor(true);
  }

  void OnBrowserWindowDidBecomeInactive(BrowserWindowInterface* bwi) {
    UpdateInkdropHoverColor(false);
  }

  void UpdateInkdropHoverColor(bool is_frame_active) {
    SetInkdropHoverColorId(is_frame_active
                               ? kColorTabBackgroundInactiveHoverFrameActive
                               : kColorTabBackgroundInactiveHoverFrameInactive);
    UpdateColors();
  }

  float GetWidthFactor() const { return width_factor_; }

  void SetWidthFactor(float factor) {
    width_factor_ = factor;
    this->PreferredSizeChanged();
  }

 protected:
  virtual void Init(base::RepeatingClosure expansion_animation_done_callback,
                    const std::u16string& tooltip) {
    // Set the label text if not already set
    if (this->label()->GetText().empty()) {
      this->label()->SetText(GetLabelText());
    }
    width_animation_controller_ =
        std::make_unique<GlicButton<T>::WidthAnimationController>(
            *this, std::move(expansion_animation_done_callback));
    this->SetProperty(views::kElementIdentifierKey, kGlicButtonElementId);
    this->SetPaintToLayer();
    this->layer()->SetFillsBoundsOpaquely(false);

    UpdateIcon();
    OnLabelVisibilityChanged();
    auto* image_view =
        static_cast<views::ImageView*>(this->image_container_view());
    image_view->SetImageSize({icon_size_, icon_size_});
    image_view->SetPaintToLayer();
    image_view->layer()->SetFillsBoundsOpaquely(false);

    CreateIconAndLabelContainer();

    if (!this->label()->layer()) {
      // Make sure label() has a layer even if its text is empty, so we can use
      // the same opacity animation whether or not the label has text.
      this->label()->SetPaintToLayer();
    }
    this->label()->SetProperty(views::kMarginsKey,
                               gfx::Insets().set_right(kLabelRightMargin));

    if (close_button() != nullptr) {
      close_button()->SetProperty(
          views::kMarginsKey,
          gfx::Insets().set_left_right(
              HighlightNudgeEnabled() ? kCloseButtonMargin : 0,
              kCloseButtonMargin));
      SetCloseButtonVisible(false);
    }

    this->SetTooltipText(tooltip);
    this->GetViewAccessibility().SetName(tooltip);

    SetDefaultColors();
    UpdateColors();
    this->SetVisible(true);

    this->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

    auto* const layout_manager =
        this->SetLayoutManager(std::make_unique<views::BoxLayout>());
    layout_manager->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kStart);
  }

  void UpdateIcon() override {
    const bool solid_icon_for_pressed_state =
        base::FeatureList::IsEnabled(features::kGlicButtonPressedState) &&
        features::kGlicButtonPressedForceSolidIcon.Get() && glic_panel_is_open_;
    const ui::ImageModel& model =
        (solid_icon_for_pressed_state || IsHighlightVisible())
            ? icon_for_highlight_
            : normal_icon_;

    this->SetImageModel(views::Button::STATE_NORMAL, model);
    this->SetImageModel(views::Button::STATE_HOVERED, model);
    this->SetImageModel(views::Button::STATE_PRESSED, model);
    this->SetImageModel(views::Button::STATE_DISABLED, model);
  }

  void SetForegroundFrameActiveColorId(ui::ColorId new_color_id) override {
    GlicBaseShim<T>::SetForegroundFrameActiveColorId(new_color_id);
  }
  void SetForegroundFrameInactiveColorId(ui::ColorId new_color_id) override {
    GlicBaseShim<T>::SetForegroundFrameInactiveColorId(new_color_id);
  }
  void SetBackgroundFrameActiveColorId(ui::ColorId new_color_id) override {
    GlicBaseShim<T>::SetBackgroundFrameActiveColorId(new_color_id);
  }
  void SetBackgroundFrameInactiveColorId(ui::ColorId new_color_id) override {
    GlicBaseShim<T>::SetBackgroundFrameInactiveColorId(new_color_id);
  }

  // Callback when the context menu closes.
  void OnMenuClosed() {
    menu_anchor_higlight_.reset();
    menu_runner_.reset();
  }

  PrefService* GetPrefService() { return profile_->GetPrefs(); }

  // Preferred width multiplier, between 0-1. Used to animate button size.
  raw_ptr<views::View> close_button_;

  views::View* close_button() {
    if constexpr (std::is_same_v<T, TabStripNudgeButton>) {
      return T::close_button();
    }
    return close_button_;
  }

  static std::u16string GetLabelText() {
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

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;

  // The model adapter for the context menu.
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;

  // Model for the context menu.
  std::unique_ptr<ui::MenuModel> menu_model_;

  // Used to ensure the button remains highlighted while the menu is active.
  std::optional<views::Button::ScopedAnchorHighlight> menu_anchor_higlight_;

  // Menu runner for the context menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Profile corresponding to the browser that this button is on.
  raw_ptr<Profile> profile_;

  // Icon size for Gemini Button.
  const int icon_size_ = 20;

 private:
  // views::LabelButton:
  void SetText(std::u16string_view text) override {
    if constexpr (std::is_same_v<T, ToolbarButton>) {
      // SetText is private in ToolbarButton and prefers to use SetHighlight.
      std::u16string highlight_text(text);
      this->SetHighlight(highlight_text, kTextOnHighlight);
    } else {
      this->SetText(text);
    }

    // Setting label text seems to clear the margin. Set it again.
    this->label()->SetProperty(views::kMarginsKey,
                               gfx::Insets().set_right(kLabelRightMargin));
  }

  void NotifyClick(const ui::Event& event) override {
    if (base::FeatureList::IsEnabled(features::kGlicButtonPressedState)) {
      // T likely manipulates the ink drop in its NotifyClick(), so
      // if we're using the ink drop to show the button's pressed state, skip
      // T::NotifyClick() and just call the base
      // NotifyClick().
      views::LabelButton::NotifyClick(event);
    } else {
      T::NotifyClick(event);
    }
  }

  // Creates the model for the context menu.
  std::unique_ptr<ui::SimpleMenuModel> CreateMenuModel() {
    std::unique_ptr<ui::SimpleMenuModel> model =
        std::make_unique<ui::SimpleMenuModel>(this);
    model->AddItemWithStringIdAndIcon(
        IDC_GLIC_TOGGLE_PIN, IDS_GLIC_BUTTON_CXMENU_UNPIN,
        ui::ImageModel::FromVectorIcon(kKeepOffIcon, ui::kColorIcon, 16));
    return model;
  }

  // Must be implemented by any subclass that does not have T implementing the
  // class.
  void UpdateColors() override { GlicBaseShim<T>::UpdateColors(); }

  void SetCloseButtonFocusBehavior(
      views::View::FocusBehavior focus_behavior) override {
    GlicBaseShim<T>::SetCloseButtonFocusBehavior(focus_behavior);
  }

  void SetLeftRightCornerRadii(int left, int right) override {
    GlicBaseShim<T>::SetLeftRightCornerRadii(left, right);
  }

  void SetInkdropHoverColorId(const ChromeColorIds new_color_id) override {
    GlicBaseShim<T>::SetInkdropHoverColorId(new_color_id);
  }

  // Called every time the contextual cue is shown to make a screen reader
  // announcement.
  void AnnounceNudgeShown() {
    auto announcement = l10n_util::GetStringFUTF16(
        IDS_GLIC_CONTEXTUAL_CUEING_ANNOUNCEMENT,
        GlicLauncherConfiguration::GetGlobalHotkey().GetShortcutText());
    this->GetViewAccessibility().AnnounceAlert(announcement);
  }

  void UpdateTextAndBackgroundColors() {
    if (!EntrypointVariationsEnabled()) {
      return;
    }

    const bool highlight_visible = IsHighlightVisible();
    if (highlight_visible || ShouldUseAltIcon()) {
      SetBackgroundFrameActiveColorId(ui::kColorSysBase);

      if (highlight_visible) {
        SetForegroundFrameActiveColorId(kTextOnHighlight);
        this->SetTextColor(views::Button::STATE_DISABLED,
                           kTextDisabledOnHighlight);
      } else {
        SetForegroundFrameActiveColorId(kForegroundOnAltBackground);
        this->SetTextColor(views::Button::STATE_DISABLED, kTextDisabled);
      }
    } else {
      SetBackgroundFrameActiveColorId(
          kColorNewTabButtonCRBackgroundFrameActive);
      SetForegroundFrameActiveColorId(kForeground);
      this->SetTextColor(views::Button::STATE_DISABLED, kTextDisabled);
    }

    if (base::FeatureList::IsEnabled(features::kGlicButtonPressedState) &&
        this->GetWidget()) {
      this->SetHighlighted(glic_panel_is_open_);
    }

    UpdateColors();
  }

  bool IsHighlightVisible() const {
    return HighlightNudgeEnabled() && GetIsShowingNudge() &&
           this->GetState() != views::Button::STATE_HOVERED;
  }

  void CreateIconAndLabelContainer() {
    // Restructure the button to place a "highlight" view behind the icon and
    // label. It's separate from icon_and_label_container so that its opacity
    // can be animated independently.
    //
    // parent (layout: FillLayout)
    // +-> highlight_view_
    // +-> container (layout: horizontal BoxLayout)
    //     +-> image_container_view()
    //     +-> label()

    std::optional<size_t> icon_index =
        this->GetIndexOf(this->image_container_view());
    CHECK(icon_index);
    auto* parent =
        this->AddChildViewAt(std::make_unique<views::View>(), *icon_index);
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
        this->RemoveChildViewT(this->image_container_view()));
    std::unique_ptr<views::Label> label_internal =
        this->RemoveChildViewT(this->label());

    label_internal->SetPaintToLayer();
    label_internal->SetSkipSubpixelRenderingOpacityCheck(true);
    label_internal->layer()->SetFillsBoundsOpaquely(false);
    label_internal->SetSubpixelRenderingEnabled(false);

    icon_and_label_container->AddChildView(std::move(label_internal));
  }

  void SetCloseButtonVisible(bool visible) {
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

    this->PreferredSizeChanged();
  }

  void ShowNudge() {
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
        .OnEnded(base::BindOnce(&GlicButton<T>::ApplyTextAndFadeIn,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(pending_text_),
                                /*delay=*/DurationMs(0), kNudgeFadeInDuration))
        .Once()
        .At(kNudgeFadeInStart - kLabelFadeOutDuration)
        .SetOpacity(this->label(), 0)
        .SetDuration(kLabelFadeOutDuration);
  }

  void HideNudge() {
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
        .OnEnded(base::BindOnce(&GlicButton<T>::ApplyTextAndFadeIn,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::make_optional(GetLabelText()),
                                kLabelFadeInStart, kLabelFadeInDuration))
        .Once()
        .At(kNudgeFadeOutStart)
        .SetOpacity(this->label(), kNudgeFinalOpacity)
        .SetDuration(kNudgeFadeOutDuration);
  }

  void ApplyTextAndFadeIn(std::optional<std::u16string> text,
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
      this->label()->layer()->SetOpacity(ShouldShowLabel() ? 0.5 : 0);
    }

    views::AnimationBuilder()
        .Once()
        .At(delay)
        .SetOpacity(this->label(), 1)
        .SetDuration(duration);
  }

  void MaybeFadeHighlightOnHover(float final_opacity) {
    if (GetIsShowingNudge() && HighlightNudgeEnabled()) {
      const base::TimeDelta kFadeDuration = DurationMs(170);
      views::AnimationBuilder()
          .Once()
          .SetOpacity(highlight_view_, final_opacity)
          .SetDuration(kFadeDuration);
    }
  }

  int CalculateExpandedWidth() {
    int nudge_text_width = 0;
    // May be unset in tests.
    // TODO(449773402): pending_text_ should always be set here.
    if (pending_text_) {
      // Measure the nudge text.
      auto render_text = gfx::RenderText::CreateRenderText();
      render_text->SetText(*pending_text_);
      render_text->SetFontList(this->label()->font_list());
      nudge_text_width = render_text->GetStringSize().width();
    }

    const int old_width = PreferredSize().width();
    // Replace old label with new.
    int new_width = old_width - this->label()->width() + nudge_text_width;
    if (!ShouldShowLabel()) {
      // If transitioning from empty label to nudge label, make sure the label
      // margin is included.
      new_width += kLabelRightMargin;
    }
    if (last_width_state_ == WidthState::kCollapsed) {
      // Add extra margin if the label was previously collapsed, as the
      // old_width is smaller.
      new_width += kCloseButtonMargin;
    }
    return new_width;
  }

  bool IsAnimatingTextVisibility() const {
    return width_state_ == WidthState::kCollapsed ||
           last_width_state_ == WidthState::kCollapsed;
  }

  bool IsHidingNudge() const {
    return (width_state_ == WidthState::kNormal ||
            width_state_ == WidthState::kCollapsed) &&
           last_width_state_ == WidthState::kNudge;
  }

  void SetWidthState(WidthState state) {
    last_width_state_ = width_state_;
    width_state_ = state;
  }

  gfx::Size PreferredSize() const {
    return this->GetLayoutManager()->GetPreferredSize(this);
  }

  views::View* highlight_view() { return highlight_view_; }
  WidthState width_state() { return width_state_; }

#if BUILDFLAG(ENABLE_GLIC)
  virtual void OnLabelVisibilityChanged() {}
#endif  // BUILDFLAG(ENABLE_GLIC)

  static bool EntrypointVariationsEnabled() {
    return base::FeatureList::IsEnabled(features::kGlicEntrypointVariations);
  }

  static bool ShouldShowLabel() {
    return EntrypointVariationsEnabled() &&
           features::kGlicEntrypointVariationsShowLabel.Get();
  }

  static bool ShouldUseAltIcon() {
    return EntrypointVariationsEnabled() &&
           features::kGlicEntrypointVariationsAltIcon.Get();
  }

  static const gfx::VectorIcon& GlicVectorIcon() {
    return GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON);
  }

  ui::ImageModel GetNormalIcon() {
    if (ShouldUseAltIcon()) {
      return ui::ImageModel::FromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_GLIC_BUTTON_ALT_ICON));
    }
    return ui::ImageModel::FromVectorIcon(
        GlicVectorIcon(),
        ShouldUseAltIcon() ? kForegroundOnAltBackground : kForeground,
        icon_size_);
  }

  static bool HighlightNudgeEnabled() {
    return EntrypointVariationsEnabled() &&
           features::kGlicEntrypointVariationsHighlightNudge.Get();
  }

  ui::ImageModel GetIconForHighlight() {
    return ui::ImageModel::FromVectorIcon(
        GlicVectorIcon(),
        HighlightNudgeEnabled() ? kTextOnHighlight : kForeground, icon_size_);
  }

  // Helper for making animation durations instant if animations are disabled.
  static base::TimeDelta DurationMs(int duration_ms) {
    return gfx::Animation::ShouldRenderRichAnimation()
               ? base::Milliseconds(duration_ms)
               : base::TimeDelta();
  }

  // Callback which is invoked when the button is hovered (i.e., the user is
  // more likely to interact with it soon).
  base::RepeatingClosure hovered_callback_;

  // Callback which is invoked when there is a mouse down event on the button
  // (i.e., the user is very likely to interact with it soon).
  base::RepeatingClosure mouse_down_callback_;

  // Start and end values for width animations.
  int start_width_ = 0;
  int end_width_ = 0;

  // View to be drawn behind the icon and label with a background color.
  raw_ptr<views::View> highlight_view_ = nullptr;

  // Container view for the icon and label, and the highlight drawn behind them.
  raw_ptr<views::View> icon_label_highlight_view_ = nullptr;

  // Holds the incoming nudge text until the point in the animation when it can
  // be applied.
  std::optional<std::u16string> pending_text_;

  const ui::ImageModel normal_icon_;
  const ui::ImageModel icon_for_highlight_;

  bool glic_panel_is_open_ = false;

  // Width of the button when in WidthState::kNormal, set in AddedToWidget().
  int normal_width_ = 0;
  WidthState last_width_state_ = WidthState::kNormal;
  WidthState width_state_ = WidthState::kNormal;
  // Whether or not the button was collapsed before the nudge was shown.
  bool collapsed_before_nudge_shown_ = false;

  std::unique_ptr<WidthAnimationController> width_animation_controller_;

  // Window active and inactive subscriptions for changing the hover color.
  base::CallbackListSubscription window_did_become_active_subscription_;
  base::CallbackListSubscription window_did_become_inactive_subscription_;

  float width_factor_ = 0;

  base::WeakPtrFactory<GlicButton> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_H_
