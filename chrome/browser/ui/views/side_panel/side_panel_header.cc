// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_header.h"

#include "base/functional/callback_forward.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace {

void ConfigureControlButton(views::ImageButton* button) {
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  views::InstallCircleHighlightPathGenerator(button);

  const int minimum_button_size =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_BUTTON_MINIMUM_SIZE);
  button->SetMinimumImageSize(
      gfx::Size(minimum_button_size, minimum_button_size));

  button->SetProperty(
      views::kMarginsKey,
      gfx::Insets().set_left(ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::
              DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL)));

  button->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification().WithAlignment(views::LayoutAlignment::kEnd));
}

std::unique_ptr<views::ImageView> CreateIcon() {
  std::unique_ptr<views::ImageView> icon = std::make_unique<views::ImageView>();
  const int horizontal_margin =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::
              DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL) *
      2;
  icon->SetProperty(views::kMarginsKey,
                    gfx::Insets().set_left(horizontal_margin));
  return icon;
}

std::unique_ptr<views::Label> CreateTitle() {
  std::unique_ptr<views::Label> title = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_HEADLINE_5);

  title->SetEnabledColor(kColorSidePanelEntryTitle);
  title->SetBackgroundColor(kColorToolbar);
  title->SetSubpixelRenderingEnabled(false);
  const int horizontal_margin =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::
              DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL) *
      2;
  title->SetProperty(views::kMarginsKey,
                     gfx::Insets().set_left(horizontal_margin));
  title->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithAlignment(views::LayoutAlignment::kStart));
  return title;
}

std::unique_ptr<views::ToggleImageButton> CreatePinToggleButton(
    base::RepeatingClosure pressed_callback) {
  auto button =
      std::make_unique<views::ToggleImageButton>(std::move(pressed_callback));
  views::ConfigureVectorImageButton(button.get());
  ConfigureControlButton(button.get());
  button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_HEADER_PIN_BUTTON_TOOLTIP));
  button->SetToggledTooltipText(
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_HEADER_UNPIN_BUTTON_TOOLTIP));

  int dip_size = ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE);
  const gfx::VectorIcon& pin_icon = kKeepIcon;
  const gfx::VectorIcon& unpin_icon = kKeepOffIcon;
  views::SetImageFromVectorIconWithColorId(
      button.get(), pin_icon, kColorSidePanelHeaderButtonIcon,
      kColorSidePanelHeaderButtonIconDisabled, dip_size);
  const ui::ImageModel& normal_image = ui::ImageModel::FromVectorIcon(
      unpin_icon, kColorSidePanelHeaderButtonIcon, dip_size);
  const ui::ImageModel& disabled_image = ui::ImageModel::FromVectorIcon(
      unpin_icon, kColorSidePanelHeaderButtonIconDisabled, dip_size);
  button->SetToggledImageModel(views::Button::STATE_NORMAL, normal_image);
  button->SetToggledImageModel(views::Button::STATE_DISABLED, disabled_image);
  button->SetProperty(views::kElementIdentifierKey,
                      kSidePanelPinButtonElementId);
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  // By default, the button's accessible description is set to the button's
  // tooltip text. For the pin button, we only want the accessible name to be
  // read on accessibility mode since it includes the tooltip text. Thus we set
  // the accessible description.
  button->GetViewAccessibility().SetDescription(
      std::u16string(), ax::mojom::DescriptionFrom::kAttributeExplicitlyEmpty);
  // The icon is later set as visible for side panels that support it.
  button->SetVisible(false);
  return button;
}

std::unique_ptr<views::ImageButton> CreateControlButton(
    base::RepeatingClosure pressed_callback,
    const gfx::VectorIcon& icon,
    const std::u16string& tooltip_text,
    ui::ElementIdentifier view_id,
    int dip_size) {
  auto button = views::CreateVectorImageButtonWithNativeTheme(
      pressed_callback, icon, dip_size, kColorSidePanelHeaderButtonIcon,
      kColorSidePanelHeaderButtonIconDisabled);
  button->SetTooltipText(tooltip_text);
  ConfigureControlButton(button.get());
  button->SetProperty(views::kElementIdentifierKey, view_id);
  button->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  return button;
}
}  // namespace

SidePanelHeader::SidePanelHeader(
    TogglePinStateCallback toggle_pin_state_callback,
    OpenInNewTabCallback open_in_new_tab_callback,
    OpenMoreInfoMenuCallback open_more_info_menu_callback,
    CloseSidePanelCallback close_side_panel_callback) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  auto* const layout = SetLayoutManager(std::make_unique<views::FlexLayout>());

  // Set alignments for horizontal (main) and vertical (cross) axes.
  layout->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // The minimum cross axis size should the expected height of the header.
  constexpr int kDefaultSidePanelHeaderHeight = 40;
  layout->SetMinimumCrossAxisSize(kDefaultSidePanelHeaderHeight);

  panel_icon_ = AddChildView(CreateIcon());
  panel_title_ = AddChildView(CreateTitle());

  header_pin_button_ =
      AddChildView(CreatePinToggleButton(std::move(toggle_pin_state_callback)));

  header_open_in_new_tab_button_ = AddChildView(CreateControlButton(
      std::move(open_in_new_tab_callback), kOpenInNewIcon,
      l10n_util::GetStringUTF16(IDS_ACCNAME_OPEN_IN_NEW_TAB),
      kSidePanelOpenInNewTabButtonElementId,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
  // The icon is later set as visible for side panels that support it.
  header_open_in_new_tab_button_->SetVisible(false);

  header_more_info_button_ = AddChildView(CreateControlButton(
      // Callback will not be used since a button controller is being set.
      base::RepeatingClosure(), kHelpMenuIcon,
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_HEADER_MORE_INFO_BUTTON_TOOLTIP),
      kSidePanelMoreInfoButtonElementId,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
  // The icon is later set as visible for side panels that support it.
  header_more_info_button_->SetVisible(false);

  // A menu button controller is used so that the button remains pressed while
  // the menu is open.
  header_more_info_button_->SetButtonController(
      std::make_unique<views::MenuButtonController>(
          header_more_info_button_, std::move(open_more_info_menu_callback),
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              header_more_info_button_)));

  AddChildView(CreateControlButton(
      std::move(close_side_panel_callback), views::kIcCloseIcon,
      l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_PANEL_CLOSE),
      kSidePanelCloseButtonElementId,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
}

SidePanelHeader::~SidePanelHeader() = default;

void SidePanelHeader::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  // The side panel header should draw on top of its parent's border.
  gfx::Rect contents_bounds = parent()->GetContentsBounds();

  gfx::Rect header_bounds = gfx::Rect(
      contents_bounds.x(), contents_bounds.y() - GetPreferredSize().height(),
      contents_bounds.width(), GetPreferredSize().height());

  SetBoundsRect(header_bounds);
}
