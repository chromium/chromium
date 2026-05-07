// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_H_

#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/private_ai/private_ai_service.h"
#include "chrome/browser/private_ai/private_ai_service_factory.h"
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
#include "components/private_ai/client.h"
#include "components/private_ai/features.h"
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

class BrowserWindowInterface;
class Profile;
class TabStripNudgeButton;

namespace views {
class MenuModelAdapter;
class MenuRunner;
}  // namespace views

namespace glic {
inline constexpr int kIconLeftMargin = 4;
inline constexpr int kCloseButtonMargin = 6;
inline constexpr int kLabelRightMargin = 8;
inline constexpr ui::ColorId kTextDisabled = ui::kColorLabelForegroundDisabled;

inline constexpr ui::ColorId kForeground =
    kColorNewTabButtonForegroundFrameActive;
inline constexpr ui::ColorId kForegroundOnAltBackground =
    ui::kColorSysOnSurface;

inline constexpr int kCollapsedWidth = 41;
inline constexpr int kSplitFlatEdgetRadius = 2;
inline constexpr int kSplitRoundedEdgeRadius = 10;
inline void EstablishPrivateAiConnection(Profile* profile) {
  if (!profile) {
    return;
  }
  if (base::FeatureList::IsEnabled(private_ai::kPrivateAi) &&
      base::FeatureList::IsEnabled(glic::kZeroStateSuggestionsUsePrivateAi)) {
    private_ai::PrivateAiService* private_ai_service =
        private_ai::PrivateAiServiceFactory::GetForProfile(profile);
    if (private_ai_service) {
      private_ai::Client* client = private_ai_service->GetClient();
      if (client) {
        client->EstablishConnection();
      }
    }
  }
}

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
        StartOpacityAnimationsForNudge(new_state);
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

    void StartOpacityAnimationsForNudge(WidthState new_state) {
      if (button_->close_button() == nullptr) {
        return;
      }
      const bool show = (new_state == WidthState::kNudge);
      const float final_close_button_opacity = show ? 1 : 0;
      const base::TimeDelta close_button_fade_start =
          show ? DurationMs(333) : base::TimeDelta();
      const base::TimeDelta close_button_fade_duration =
          show ? DurationMs(333) : DurationMs(117);

      views::AnimationBuilder()
          .Once()
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
                      base::RepeatingClosure expansion_animation_done_callback,
                      const std::u16string& tooltip,
                      const int icon_size,
                      BaseArgs&&... base_args)
      : GlicBaseShim<T>(std::move(base_args)...),
        browser_window_interface_(browser_window_interface),
        menu_model_(CreateMenuModel()),
        profile_(browser_window_interface
                     ? browser_window_interface->GetProfile()
                     : nullptr),
        normal_icon_(GetNormalIcon(icon_size)),
        icon_for_highlight_(GetIconForHighlight(icon_size)) {
    Init(expansion_animation_done_callback, tooltip);
  }

  GlicButton(const GlicButton&) = delete;
  GlicButton& operator=(const GlicButton&) = delete;
  ~GlicButton() override = default;

  // These functions below work together to hide the nudge label on the static
  // button when another nudge occupies the display space.
  //
  // Suppresses the default label on the glic button with a hide animation.
  virtual void Collapse() {
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
  virtual void Expand() {
    // Update state.
    if (width_state_ != WidthState::kCollapsed) {
      return;
    }
    WidthState old_width_state = width_state_;
    SetWidthState(WidthState::kNormal);

    start_width_ = kCollapsedWidth;
    end_width_ = normal_width_;
    width_animation_controller_->Start(old_width_state, width_state_);

    const base::TimeDelta kLabelFadeOutDuration = DurationMs(17);
    const base::TimeDelta kNudgeFadeInStart = DurationMs(50);
    const base::TimeDelta kNudgeFadeInDuration = DurationMs(50);
    views::AnimationBuilder()
        .OnEnded(base::BindOnce(&GlicButton<T>::ApplyTextAndFadeIn,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::make_optional(GetLabelText()),
                                /*delay=*/DurationMs(0), kNudgeFadeInDuration))
        .Once()
        .At(kNudgeFadeInStart - kLabelFadeOutDuration)
        .SetOpacity(this->label(), 0)
        .SetDuration(kLabelFadeOutDuration);
  }

  void SetNudgeLabel(std::string label) {
    // Store the new label text until the right moment in the animation to
    // update the view.
    pending_text_ = base::UTF8ToUTF16(label);

    if (width_state_ == WidthState::kNudge) {
      end_width_ = CalculateExpandedWidth();
      SetText(*pending_text_);
      this->PreferredSizeChanged();
    }
  }

  void RestoreDefaultLabel() {
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

    // Interpolate based on animation value.
    const int width = std::lerp(start, end, GetWidthFactor());
    return gfx::Size(width, height);
  }

  void StateChanged(views::Button::ButtonState old_state) override {
    T::StateChanged(old_state);

    if (old_state == views::Button::ButtonState::STATE_NORMAL &&
        this->GetState() == views::Button::ButtonState::STATE_HOVERED) {
      EstablishPrivateAiConnection(profile_);
    }

    UpdateTextAndBackgroundColors();
    UpdateIcon();
  }

  void AddedToWidget() override {
    // Both TabStripControlButton and parent LabelButton set up similar logic
    // here for drawing the button as enabled or disabled when window
    // activation changes. Use LabelButton's as TabStripControlButton fails to
    // update the text color when the window goes from inactive to active.
    // TODO(crbug.com/452116005): Make this behavior configurable on
    // TabStripControlButton.
    views::LabelButton::AddedToWidget();

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
    if (IsHidingNudge()) {
      SetCloseButtonVisible(false);
    }

    last_width_state_ = width_state_;
    OnLabelVisibilityChanged();
  }

  bool GetLabelEnabledForTesting() const { return this->label()->GetEnabled(); }

  // Updates the background painter to match the current border insets.
  void RefreshBackground() { UpdateColors(); }

  // Show or hide the split button styling, used when the task indicator is
  // present.
  void SetSplitButtonCornerStyling() {
    SetLeftRightCornerRadii(GetSplitRoundedEdgeRadius(), kSplitFlatEdgetRadius);
  }

  virtual void ResetSplitButtonCornerStyling() = 0;

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

  // Subclasses should override GetWidthFactor with their own implementation of
  // width_factor_.
  float GetWidthFactor() const override { return 0.0f; }

  virtual int GetSplitRoundedEdgeRadius() { return kSplitRoundedEdgeRadius; }

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

    auto* image_view =
        static_cast<views::ImageView*>(this->image_container_view());
    image_view->SetPaintToLayer();
    image_view->layer()->SetFillsBoundsOpaquely(false);

    this->label()->SetPaintToLayer();
    this->label()->layer()->SetFillsBoundsOpaquely(false);
    this->label()->SetSubpixelRenderingEnabled(false);

    if (!this->label()->layer()) {
      // Make sure label() has a layer even if its text is empty, so we can use
      // the same opacity animation whether or not the label has text.
      this->label()->SetPaintToLayer();
    }
    if (close_button() != nullptr) {
      close_button()->SetProperty(
          views::kMarginsKey,
          gfx::Insets().set_left_right(0, kCloseButtonMargin));
      SetCloseButtonVisible(false);
    }

    this->SetTooltipText(tooltip);
    this->GetViewAccessibility().SetName(tooltip);

    SetDefaultColors();

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
        solid_icon_for_pressed_state ? icon_for_highlight_ : normal_icon_;

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
          return l10n_util::GetStringUTF16(
              IDS_GLIC_BUTTON_ENTRYPOINT_ASK_GEMINI_LABEL);
  }

  bool IsAnimatingTextVisibility() const {
    return width_state_ == WidthState::kCollapsed ||
           last_width_state_ == WidthState::kCollapsed;
  }

  void SetLeftRightCornerRadii(int left, int right) override {
    GlicBaseShim<T>::SetLeftRightCornerRadii(left, right);
  }

  virtual void SetLabelMargins() {
    int right = kLabelRightMargin;
    if ((!close_button() || !close_button()->GetVisible())) {
      right += 4;
    }
    this->label()->SetProperty(views::kMarginsKey,
                               gfx::Insets().set_right(right));
  }

  // Must be implemented by any subclass that does not have T implementing the
  // class.
  void UpdateColors() override { GlicBaseShim<T>::UpdateColors(); }

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

  std::unique_ptr<WidthAnimationController> width_animation_controller_;

  WidthState width_state_ = WidthState::kNormal;

 private:
  // views::LabelButton:
  void SetText(std::u16string_view text) override {
    T::SetText(text);
    // Setting label text seems to clear the margin. Set it again.
    SetLabelMargins();
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

  void SetCloseButtonFocusBehavior(
      views::View::FocusBehavior focus_behavior) override {
    GlicBaseShim<T>::SetCloseButtonFocusBehavior(focus_behavior);
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
    SetBackgroundFrameActiveColorId(ui::kColorSysBase);
    SetForegroundFrameActiveColorId(kForegroundOnAltBackground);
    this->SetTextColor(views::Button::STATE_DISABLED, kTextDisabled);

    if (base::FeatureList::IsEnabled(features::kGlicButtonPressedState) &&
        this->GetWidget()) {
      this->SetHighlighted(glic_panel_is_open_);
    }

    UpdateColors();
  }

  void SetCloseButtonVisible(bool visible) {
    if (close_button() == nullptr) {
      return;
    }

    close_button()->SetVisible(visible);
    SetLabelMargins();
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

    // Remember the button's original width before changing the text and showing
    // the close button.
    start_width_ = PreferredSize().width();
    SetCloseButtonVisible(true);
    end_width_ = CalculateExpandedWidth();

    width_animation_controller_->Start(old_width_state, width_state_);

    const base::TimeDelta kLabelFadeOutDuration = DurationMs(17);
    const base::TimeDelta kNudgeFadeInStart = DurationMs(267);
    const base::TimeDelta kNudgeFadeInDuration = DurationMs(100);
    views::AnimationBuilder()
        .OnEnded(base::BindOnce(&GlicButton<T>::ApplyTextAndFadeIn,
                                weak_ptr_factory_.GetWeakPtr(),
                                /*text=*/std::nullopt,
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

    start_width_ = PreferredSize().width();
    end_width_ = normal_width_;
    width_animation_controller_->Start(old_width_state, width_state_);

    const base::TimeDelta kNudgeFadeOutStart = DurationMs(0);
    const base::TimeDelta kNudgeFadeOutDuration = DurationMs(133);
    const float kNudgeFinalOpacity = 0.5;
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
    if (width_state_ == WidthState::kNudge && pending_text_) {
      SetText(*pending_text_);
    } else if (text) {
      SetText(*text);
    }

    // This moment coincides with the highlight being midway through its opacity
    // animation. Update text and icon for the final highlight state now.
    UpdateTextAndBackgroundColors();
    UpdateIcon();

    if (width_state_ == WidthState::kNudge) {
      // Start at 50% opacity if replacing default label with nudge.
      this->label()->layer()->SetOpacity(0.5);
    }

    views::AnimationBuilder()
        .Once()
        .At(delay)
        .SetOpacity(this->label(), 1)
        .SetDuration(duration);
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
    if (last_width_state_ == WidthState::kCollapsed) {
      // Add extra margin if the label was previously collapsed, as the
      // old_width is smaller.
      new_width += kCloseButtonMargin;
    }
    return new_width;
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

  WidthState width_state() { return width_state_; }

  virtual void OnLabelVisibilityChanged() {}

  static const gfx::VectorIcon& GlicVectorIcon() {
    return GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON);
  }

  ui::ImageModel GetNormalIcon(const int icon_size) {
    return ui::ImageModel::FromImageSkia(
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_GLIC_BUTTON_ALT_ICON));
  }

  ui::ImageModel GetIconForHighlight(const int icon_size) {
    return ui::ImageModel::FromVectorIcon(GlicVectorIcon(), kForeground,
                                          icon_size);
  }

  // Helper for making animation durations instant if animations are disabled.
  static base::TimeDelta DurationMs(int duration_ms) {
    return gfx::Animation::ShouldRenderRichAnimation()
               ? base::Milliseconds(duration_ms)
               : base::TimeDelta();
  }

  // Start and end values for width animations.
  int start_width_ = 0;
  int end_width_ = 0;

  // Holds the incoming nudge text until the point in the animation when it can
  // be applied.
  std::optional<std::u16string> pending_text_;

  const ui::ImageModel normal_icon_;
  const ui::ImageModel icon_for_highlight_;

  bool glic_panel_is_open_ = false;

  // Width of the button when in WidthState::kNormal, set in AddedToWidget().
  int normal_width_ = 0;
  WidthState last_width_state_ = WidthState::kNormal;
  // Whether or not the button was collapsed before the nudge was shown.
  bool collapsed_before_nudge_shown_ = false;

  // Window active and inactive subscriptions for changing the hover color.
  base::CallbackListSubscription window_did_become_active_subscription_;
  base::CallbackListSubscription window_did_become_inactive_subscription_;

  base::WeakPtrFactory<GlicButton> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_BUTTON_H_
