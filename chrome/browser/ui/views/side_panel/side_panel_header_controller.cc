// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_header_controller.h"

#include <memory>
#include <string_view>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_pinning_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"

namespace {
void ConfigureControlButton(views::ImageButton* button) {
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  views::InstallCircleHighlightPathGenerator(button);
  const int minimum_button_size =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_BUTTON_MINIMUM_SIZE);
  button->SetMinimumImageSize(
      gfx::Size(minimum_button_size, minimum_button_size));
}

std::unique_ptr<views::ImageButton> CreateImageButton(
    base::RepeatingClosure callback,
    const gfx::VectorIcon& icon) {
  auto image_button = views::CreateVectorImageButtonWithNativeTheme(
      std::move(callback), icon,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE),
      kColorSidePanelHeaderButtonIcon, kColorSidePanelHeaderButtonIconDisabled);
  ConfigureControlButton(image_button.get());
  return image_button;
}
}  // namespace

SidePanelHeaderController::SidePanelHeaderController(
    Browser* browser,
    SidePanelToolbarPinningController* side_panel_toolbar_pinning_controller,
    SidePanelEntry* side_panel_entry)
    : browser_(browser),
      side_panel_toolbar_pinning_controller_(
          side_panel_toolbar_pinning_controller),
      side_panel_entry_(side_panel_entry->GetWeakPtr()) {
  CHECK(side_panel_entry_);
  actions::ActionItem* const action_item =
      SidePanelUtil::GetActionItem(browser, side_panel_entry->key());
  action_item_controller_subscription_ = action_item->AddActionChangedCallback(
      base::BindRepeating(&SidePanelHeaderController::OnActionItemChanged,
                          base::Unretained(this)));
  side_panel_toolbar_pinning_controller_observation_.Observe(
      side_panel_toolbar_pinning_controller);
}

SidePanelHeaderController::~SidePanelHeaderController() = default;

std::unique_ptr<views::ImageView> SidePanelHeaderController::CreatePanelIcon() {
  CHECK(!panel_icon_);
  CHECK(side_panel_entry_);

  std::unique_ptr<views::ImageView> icon_image_view =
      std::make_unique<views::ImageView>();
  const bool is_visible =
      side_panel_entry_->key().id() == SidePanelEntryId::kExtension;
  icon_image_view->SetVisible(is_visible);
  if (is_visible) {
    icon_image_view->SetImage(GetIconImage());
  }
  panel_icon_ = icon_image_view.get();
  return icon_image_view;
}

std::unique_ptr<views::Label> SidePanelHeaderController::CreatePanelTitle() {
  CHECK(!panel_title_);
  CHECK(side_panel_entry_);

  std::unique_ptr<views::Label> title = std::make_unique<views::Label>(
      GetTitleText(), views::style::CONTEXT_LABEL,
      views::style::STYLE_HEADLINE_5);

  title->SetEnabledColor(kColorSidePanelEntryTitle);
  title->SetBackgroundColor(kColorToolbar);
  title->SetSubpixelRenderingEnabled(false);
  panel_title_ = title.get();
  return title;
}

std::unique_ptr<views::ToggleImageButton>
SidePanelHeaderController::CreatePinButton() {
  CHECK(!pin_button_);
  CHECK(side_panel_entry_);

  auto button = std::make_unique<views::ToggleImageButton>(base::BindRepeating(
      &SidePanelHeaderController::UpdatePinState, base::Unretained(this)));
  pin_button_ = button.get();

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

  UpdatePinButton();
  return button;
}

std::unique_ptr<views::ImageButton>
SidePanelHeaderController::CreateOpenNewTabButton() {
  CHECK(!open_new_tab_button_);
  CHECK(side_panel_entry_);

  auto button = CreateImageButton(
      base::BindRepeating(&SidePanelHeaderController::OpenInNewTab,
                          base::Unretained(this)),
      kOpenInNewIcon);

  button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ACCNAME_OPEN_IN_NEW_TAB));
  button->SetVisible(side_panel_entry_->SupportsNewTabButton() &&
                     side_panel_entry_->GetOpenInNewTabURL().is_valid());
  button->SetProperty(views::kElementIdentifierKey,
                      kSidePanelOpenInNewTabButtonElementId);

  open_new_tab_button_ = button.get();
  return button;
}

std::unique_ptr<views::ImageButton>
SidePanelHeaderController::CreateMoreInfoButton() {
  CHECK(!more_info_button_);
  CHECK(side_panel_entry_);

  // Callback will not be used since a button controller is being set.
  auto button = CreateImageButton(base::RepeatingClosure(), kHelpMenuIcon);
  button->SetVisible(side_panel_entry_->SupportsMoreInfoButton());
  button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_SIDE_PANEL_HEADER_MORE_INFO_BUTTON_TOOLTIP));
  button->SetProperty(views::kElementIdentifierKey,
                      kSidePanelMoreInfoButtonElementId);

  // A menu button controller is used so that the button remains pressed while
  // the menu is open.
  button->SetButtonController(std::make_unique<views::MenuButtonController>(
      button.get(),
      base::BindRepeating(&SidePanelHeaderController::OpenMoreInfoMenu,
                          base::Unretained(this)),
      std::make_unique<views::Button::DefaultButtonControllerDelegate>(
          button.get())));

  more_info_button_ = button.get();
  return button;
}

std::unique_ptr<views::ImageButton>
SidePanelHeaderController::CreateCloseButton() {
  CHECK(!close_button_);
  CHECK(side_panel_entry_);

  auto button =
      CreateImageButton(base::BindRepeating(&SidePanelHeaderController::Close,
                                            base::Unretained(this)),
                        views::kIcCloseIcon);
  button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ACCNAME_SIDE_PANEL_CLOSE));
  button->SetProperty(views::kElementIdentifierKey,
                      kSidePanelCloseButtonElementId);
  close_button_ = button.get();
  return button;
}

void SidePanelHeaderController::OnPinStateChanged() {
  if (side_panel_entry_) {
    UpdateSidePanelHeader();
  }
}

void SidePanelHeaderController::OnActionItemChanged() {
  if (side_panel_entry_) {
    UpdateSidePanelHeader();
  }
}

void SidePanelHeaderController::UpdateSidePanelHeader() {
  CHECK(side_panel_entry_);
  panel_title_->SetText(GetTitleText());
  const bool show_icon =
      side_panel_entry_->key().id() == SidePanelEntryId::kExtension;
  panel_icon_->SetVisible(show_icon);
  if (show_icon) {
    panel_icon_->SetImage(GetIconImage());
  }
  UpdatePinButton();
  open_new_tab_button_->SetVisible(
      side_panel_entry_->SupportsNewTabButton() &&
      side_panel_entry_->GetOpenInNewTabURL().is_valid());
  more_info_button_->SetVisible(side_panel_entry_->SupportsMoreInfoButton());
}

void SidePanelHeaderController::UpdatePinButton() {
  CHECK(side_panel_entry_);
  actions::ActionItem* const action_item =
      SidePanelUtil::GetActionItem(browser_, side_panel_entry_->key());
  Profile* const profile = browser_->profile();
  const bool current_pinned_state =
      side_panel_toolbar_pinning_controller_->GetPinnedStateFor(
          side_panel_entry_->key());
  pin_button_->SetToggled(current_pinned_state);
  pin_button_->SetVisible(
      !profile->IsIncognitoProfile() && !profile->IsGuestSession() &&
      action_item->GetProperty(actions::kActionItemPinnableKey) ==
          static_cast<int>(actions::ActionPinnableState::kPinnable));

  if (!current_pinned_state) {
    // Show IPH for side panel pinning icon.
    MaybeQueuePinPromo(side_panel_entry_->key().id());
  }
}

ui::ImageModel SidePanelHeaderController::GetIconImage() {
  CHECK(side_panel_entry_);
  ui::ImageModel icon =
      SidePanelUtil::GetActionItem(browser_, side_panel_entry_->key())
          ->GetImage();
  if (icon.IsVectorIcon()) {
    icon = ui::ImageModel::FromVectorIcon(*icon.GetVectorIcon().vector_icon(),
                                          kColorSidePanelEntryIcon,
                                          icon.GetVectorIcon().icon_size());
  }
  return icon;
}

std::u16string_view SidePanelHeaderController::GetTitleText() {
  CHECK(side_panel_entry_);
  return side_panel_entry_->GetProperty(kShouldShowTitleInSidePanelHeaderKey)
             ? SidePanelUtil::GetActionItem(browser_, side_panel_entry_->key())
                   ->GetText()
             : std::u16string_view();
}

void SidePanelHeaderController::UpdatePinState() {
  if (!side_panel_entry_) {
    return;
  }

  side_panel_toolbar_pinning_controller_->UpdatePinState(
      side_panel_entry_->key());
  pin_button_->GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      side_panel_toolbar_pinning_controller_->GetPinnedStateFor(
          side_panel_entry_->key())
          ? IDS_SIDE_PANEL_PINNED
          : IDS_SIDE_PANEL_UNPINNED));
  // Close/cancel IPH for side panel pinning, if shown.
  MaybeEndPinPromo(/*pinned=*/true);
}

void SidePanelHeaderController::OpenInNewTab() {
  if (!side_panel_entry_) {
    return;
  }

  GURL new_tab_url = side_panel_entry_->GetOpenInNewTabURL();
  if (!new_tab_url.is_valid()) {
    return;
  }

  base::WeakPtr<SidePanelHeaderController> weak_this =
      weak_pointer_factor_.GetWeakPtr();
  SidePanelUtil::RecordNewTabButtonClicked(side_panel_entry_->key().id());
  content::OpenURLParams params(new_tab_url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                /*is_renderer_initiated=*/false);
  browser_->OpenURL(params, /*navigation_handle_callback=*/{});

  // `this` can be destroyed because the side panel might be closed when
  // opening a new tab. If `this` is still alive, close the side panel.
  if (weak_this) {
    Close();
  }
}

void SidePanelHeaderController::OpenMoreInfoMenu() {
  if (!side_panel_entry_) {
    return;
  }

  more_info_menu_model_ = side_panel_entry_->GetMoreInfoMenuModel();
  CHECK(more_info_menu_model_);
  menu_runner_ = std::make_unique<views::MenuRunner>(
      more_info_menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(more_info_button_->GetWidget(),
                          static_cast<views::MenuButtonController*>(
                              more_info_button_->button_controller()),
                          more_info_button_->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::mojom::MenuSourceType::kNone);
}

void SidePanelHeaderController::Close() {
  if (!side_panel_entry_) {
    return;
  }

  browser_->GetFeatures().side_panel_ui()->Close(side_panel_entry_->type());
}

void SidePanelHeaderController::MaybeQueuePinPromo(SidePanelEntryId id) {
  // Which feature is shown depends on the specific side panel that is showing.
  const base::Feature* const iph_feature =
      (id == SidePanelEntryId::kLensOverlayResults)
          ? &feature_engagement::kIPHSidePanelLensOverlayPinnableFeature
          : &feature_engagement::kIPHSidePanelGenericPinnableFeature;

  // If the desired promo hasn't changed, there's nothing to do.
  if (pending_pin_promo_ == iph_feature) {
    return;
  }

  // End or cancel the current promo.
  if (pending_pin_promo_) {
    MaybeEndPinPromo(/*pinned=*/false);
  }

  // Queue up the next promo to be shown, if there is one that can be shown.
  pending_pin_promo_ = iph_feature;
  if (iph_feature && !BrowserUserEducationInterface::From(browser_)
                          ->CanShowFeaturePromo(*iph_feature)
                          .is_blocked_this_instance()) {
    // Default to ten second delay, but allow setting a different parameter via
    // field trial.
    const base::TimeDelta delay = base::GetFieldTrialParamByFeatureAsTimeDelta(
        *iph_feature, "x_custom_iph_delay", base::Seconds(10));
    pin_promo_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&SidePanelHeaderController::ShowPinPromo,
                       base::Unretained(this)));
  }
}

void SidePanelHeaderController::ShowPinPromo() {
  if (!pending_pin_promo_) {
    return;
  }

  BrowserUserEducationInterface::From(browser_)->MaybeShowFeaturePromo(
      *pending_pin_promo_);
}

void SidePanelHeaderController::MaybeEndPinPromo(bool pinned) {
  if (!pending_pin_promo_) {
    return;
  }

  auto* const user_education = BrowserUserEducationInterface::From(browser_);
  if (pinned) {
    user_education->NotifyFeaturePromoFeatureUsed(
        *pending_pin_promo_,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    if (pending_pin_promo_ ==
        &feature_engagement::kIPHSidePanelLensOverlayPinnableFeature) {
      user_education->MaybeShowFeaturePromo(
          feature_engagement::kIPHSidePanelLensOverlayPinnableFollowupFeature);
    }
  } else {
    user_education->AbortFeaturePromo(*pending_pin_promo_);
  }

  pin_promo_timer_.Stop();
  pending_pin_promo_ = nullptr;
}
