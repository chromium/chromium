// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/glic_button.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
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
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace glic {
namespace {

constexpr int kHighlightMargin = 2;
constexpr int kHighlightCornerRadius = 8;
constexpr int kLabelRightMargin = 8;
constexpr int kHighlightCloseButtonLeftMargin = 4;
constexpr int kHighlightCloseButtonRightMargin = 8;
constexpr ui::ColorId kHighlightColorId = ui::kColorSysPrimary;
constexpr ui::ColorId kTextOnHighlight = ui::kColorSysOnPrimary;
constexpr ui::ColorId kDefaultTextColorV2 = ui::kColorSysOnSurfacePrimary;

constexpr int kIconSize = 16;

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
  return ui::ImageModel::FromVectorIcon(GlicVectorIcon(),
                                        ui::kColorSysOnSurface, kIconSize);
}

ui::ImageModel GetIconForHighlight() {
  if (HighlightNudgeEnabled()) {
    return ui::ImageModel::FromVectorIcon(GlicVectorIcon(), kTextOnHighlight,
                                          kIconSize);
  }
  return {};
}

gfx::Insets GetIconMargins() {
  // Default margins for the icon. Right margin comes from the button layout
  // manager's internal spacing.
  int left = 6, right = 0;

  if (ShouldShowLabel()) {
    // Extra left margin if the label is shown.
    left += 2;
  }

  if (HighlightNudgeEnabled()) {
    // Some of the icon's apparent left margin comes from the highlight view
    // (its parent).
    left -= kHighlightMargin;
    // Set our own right margin since the button isn't responsible for this
    // spacing when the highlight is shown.
    right = 4;
  }
  return gfx::Insets().set_left_right(left, right);
}

std::unique_ptr<views::View> CreateHighlightView() {
  auto view = std::make_unique<views::View>();
  view->SetBackground(views::CreateRoundedRectBackground(
      SK_ColorTRANSPARENT, kHighlightCornerRadius, 0));
  view->SetProperty(views::kMarginsKey, gfx::Insets(kHighlightMargin));
  // Don't steal hover events
  view->SetCanProcessEventsWithinSubtree(false);

  auto* const layout_manager =
      view->SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  return view;
}

}  // namespace

GlicButton::GlicButton(TabStripController* tab_strip_controller,
                       PressedCallback pressed_callback,
                       PressedCallback close_pressed_callback,
                       base::RepeatingClosure hovered_callback,
                       base::RepeatingClosure mouse_down_callback,
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
      normal_icon_(GetNormalIcon()),
      icon_for_highlight_(GetIconForHighlight()) {
  SetProperty(views::kElementIdentifierKey, kGlicButtonElementId);

  UpdateIcon();
  auto* image_view = static_cast<views::ImageView*>(image_container_view());
  image_view->SetImageSize({kIconSize, kIconSize});
  image_view->SetProperty(views::kMarginsKey, GetIconMargins());

  if (HighlightNudgeEnabled()) {
    std::optional<size_t> icon_index = GetIndexOf(image_container_view());
    CHECK(icon_index);
    highlight_view_ = AddChildViewAt(CreateHighlightView(), *icon_index);

    // Reparent icon and label under the highlight.
    highlight_view_->AddChildView(RemoveChildViewT(image_container_view()));
    highlight_view_->AddChildView(RemoveChildViewT(label()));

    close_button()->SetProperty(
        views::kMarginsKey,
        gfx::Insets().set_left_right(kHighlightCloseButtonLeftMargin,
                                     kHighlightCloseButtonRightMargin));
  }

  set_context_menu_controller(this);

  SetTooltipText(tooltip);
  GetViewAccessibility().SetName(tooltip);

  SetDefaultColors();
  UpdateColors();

  SetVisible(true);
  SetIsShowingNudge(false);
  SetHighlightOpacity(0);

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

void GlicButton::SetNudgeLabel(std::string label) {
  SetText(base::UTF8ToUTF16(label));
}

void GlicButton::RestoreDefaultLabel() {
  SetText(GetLabelText());
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
    SetCloseButtonVisible(true);
    SetCloseButtonFocusBehavior(FocusBehavior::ALWAYS);
    AnnounceNudgeShown();
  } else {
    SetCloseButtonVisible(false);
    SetCloseButtonFocusBehavior(FocusBehavior::NEVER);
  }

  if (highlight_view_) {
    label()->SetProperty(views::kMarginsKey,
                         gfx::Insets().set_right(kLabelRightMargin));
  }

  is_showing_nudge_ = is_showing;
  UpdateTextAndBackgroundColors();
  UpdateIcon();
  PreferredSizeChanged();
}

void GlicButton::SetWidthFactor(float factor) {
  TabStripNudgeButton::SetWidthFactor(factor);
  SetHighlightOpacity(factor);
}

void GlicButton::OnAnimationEnded() {
  if (GetWidthFactor() == 0) {
    RestoreDefaultLabel();
  }
}

void GlicButton::SetHighlightOpacity(float fraction) {
  if (views::Background* highlight_background =
          highlight_view_ ? highlight_view_->background() : nullptr;
      highlight_background && GetColorProvider()) {
    // Animate highlight background between transparent and opaque.
    SkColor highlight_color = ui::ColorVariant(kHighlightColorId)
                                  .ResolveToSkColor(GetColorProvider());
    highlight_color = SkColorSetA(highlight_color, fraction * SK_AlphaOPAQUE);
    highlight_background->SetColor(highlight_color);
  }
}

gfx::Size GlicButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int full_width =
      GetLayoutManager()->GetPreferredSize(this, available_size).width();

  const int height = TabStripControlButton::CalculatePreferredSize(
                         views::SizeBounds(full_width, available_size.height()))
                         .height();
  const int collapsed_width = std::max(initial_width_, height);
  const int width = std::lerp(collapsed_width, full_width, GetWidthFactor());

  return gfx::Size(width, height);
}

void GlicButton::StateChanged(ButtonState old_state) {
  TabStripNudgeButton::StateChanged(old_state);
  if (old_state == STATE_NORMAL && GetState() == STATE_HOVERED &&
      hovered_callback_) {
    hovered_callback_.Run();
  }

  UpdateTextAndBackgroundColors();
  UpdateIcon();
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
}

bool GlicButton::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && mouse_down_callback_) {
    mouse_down_callback_.Run();
    return true;
  }
  return false;
}

void GlicButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (ShouldShowLabel() && !initial_width_) {
    initial_width_ = GetLayoutManager()->GetPreferredSize(this).width();
  }
  TabStripNudgeButton::OnBoundsChanged(previous_bounds);
}

void GlicButton::AddedToWidget() {
  TabStripNudgeButton::AddedToWidget();
  // Make sure properties like highlight color are updated after this view is
  // added to the hierarchy.
  SetIsShowingNudge(is_showing_nudge_);
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

void GlicButton::HighlightGlicButton() {
  SetBackgroundFrameActiveColorId(kColorTabBackgroundInactiveHoverFrameActive);
  SetBackgroundFrameInactiveColorId(
      kColorTabBackgroundInactiveHoverFrameInactive);
}

void GlicButton::SetDefaultColors() {
  SetForegroundFrameActiveColorId(
      EntrypointVariationsEnabled() ? kDefaultTextColorV2
                                    : kColorNewTabButtonForegroundFrameActive);
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
    } else {
      SetForegroundFrameActiveColorId(kDefaultTextColorV2);
    }
  } else {
    SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
    SetForegroundFrameActiveColorId(kDefaultTextColorV2);
  }

  UpdateColors();
}

void GlicButton::UpdateIcon() {
  const ui::ImageModel& model =
      IsHighlightVisible() ? icon_for_highlight_ : normal_icon_;

  SetImageModel(views::Button::STATE_NORMAL, model);
  SetImageModel(views::Button::STATE_HOVERED, model);
  SetImageModel(views::Button::STATE_PRESSED, model);
}

bool GlicButton::IsHighlightVisible() const {
  return highlight_view_ && is_showing_nudge_ && GetState() != STATE_HOVERED;
}

BEGIN_METADATA(GlicButton)
END_METADATA

}  // namespace glic
