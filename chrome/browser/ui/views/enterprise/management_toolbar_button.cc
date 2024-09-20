// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/enterprise/management_toolbar_button.h"

#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr int kButtonMaxWidth = 180;

bool CanShowManagementToolbarButton(Profile* profile) {
  const auto* pref_service = profile->GetPrefs();
  if (!pref_service) {
    return false;
  }

  // Show the button if a label or icon is specified.
  if (!pref_service->GetString(prefs::kEnterpriseCustomLabel).empty() ||
      !pref_service->GetString(prefs::kEnterpriseLogoUrl).empty()) {
    return true;
  }

  auto* profile_management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);
  if (!profile_management_service) {
    return false;
  }

  const bool profile_managed = profile_management_service->IsManaged();

  // Show the button if the profile has any policies applied.
  if (base::FeatureList::IsEnabled(features::kManagementToolbarButton)) {
    return profile_managed;
  }

  // Show the button if the profile has any policy appplied and the profile or
  // device is managed from a trusted source.
  if (base::FeatureList::IsEnabled(
          features::kManagementToolbarButtonForTrustedManagementSources)) {
    const bool trusted_management =
        profile_managed &&
        (profile_management_service->GetManagementAuthorityTrustworthiness() >=
             policy::ManagementAuthorityTrustworthiness::TRUSTED ||
         policy::ManagementServiceFactory::GetForPlatform()
                 ->GetManagementAuthorityTrustworthiness() >=
             policy::ManagementAuthorityTrustworthiness::TRUSTED);
    return trusted_management;
  }

  return false;
}

}  // namespace

ManagementToolbarButton::ManagementToolbarButton(BrowserView* browser_view,
                                                 Profile* profile)
    : ToolbarButton(base::BindRepeating(&ManagementToolbarButton::ButtonPressed,
                                        base::Unretained(this))),
      browser_(browser_view->browser()),
      profile_(profile) {
  // Activate on press for left-mouse-button only to mimic other MenuButtons
  // without drag-drop actions (specifically the adjacent browser menu).
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);
  SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON);

  SetID(VIEW_ID_MANAGEMENT_BUTTON);
  SetProperty(views::kElementIdentifierKey, kToolbarManagementButtonElementId);

  // The icon should not flip with RTL UI. This does not affect text rendering
  // and LabelButton image/label placement is still flipped like usual.
  SetFlipCanvasOnPaintForRTLUI(false);

  GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);

  // We need to have the icon on the left and the (potential) management
  // label on the right.
  SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetLabelStyle(views::style::STYLE_BODY_4_MEDIUM);

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kEnterpriseCustomLabel,
      base::BindRepeating(&ManagementToolbarButton::UpdateManagementInfo,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kEnterpriseLogoUrl,
      base::BindRepeating(&ManagementToolbarButton::UpdateManagementInfo,
                          base::Unretained(this)));
  SetVisible(false);
  SetMaxSize(gfx::Size(kButtonMaxWidth, 0));
  SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
  UpdateManagementInfo();
}

ManagementToolbarButton::~ManagementToolbarButton() = default;

void ManagementToolbarButton::UpdateManagementInfo() {
  PrefService* prefs = profile_->GetPrefs();
  std::string label;
  std::string icon_url;
  if (prefs->HasPrefPath(prefs::kEnterpriseLogoUrl)) {
    icon_url = prefs->GetString(prefs::kEnterpriseLogoUrl);
  }
  // If no icon is set at profile level but the browser and profile are managed
  // by the same entity, use the browser level icon.
  if (icon_url.empty() &&
      chrome::AreProfileAndBrowserManagedBySameEntity(profile_)) {
    icon_url =
        g_browser_process->local_state()->GetString(prefs::kEnterpriseLogoUrl);
  }
  bool show_management_toolbar_button =
      CanShowManagementToolbarButton(profile_);
  bool button_becoming_visible =
      !GetVisible() && show_management_toolbar_button;
  SetVisible(show_management_toolbar_button);
  if (button_becoming_visible && browser_ && browser_->window()) {
    browser_->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHToolbarManagementButtonFeature);
  }
  SetManagementLabel(prefs->GetString(prefs::kEnterpriseCustomLabel));
  if (show_management_toolbar_button) {
    enterprise_util::GetManagementIcon(
        GURL(icon_url), profile_,
        base::BindOnce(&ManagementToolbarButton::SetManagementIcon,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    management_icon_ = gfx::Image();
  }
}

void ManagementToolbarButton::UpdateIcon() {
  // If widget isn't set, the button doesn't have access to the theme provider
  // to set colors. Defer updating until AddedToWidget().
  if (!GetWidget()) {
    return;
  }

  SetImageModel(ButtonState::STATE_NORMAL, GetIcon());
}

void ManagementToolbarButton::Layout(PassKey) {
  LayoutSuperclass<ToolbarButton>(this);

  // TODO(crbug.com/40699569): this is a hack to avoid mismatch between icon
  // bitmap scaling and DIP->canvas pixel scaling in fractional DIP scaling
  // modes (125%, 133%, etc.) that can cause the right-hand or bottom pixel row
  // of the icon image to be sliced off at certain specific browser sizes and
  // configurations.
  //
  // In order to solve this, we increase the width and height of the image by 1
  // after layout, so the rest of the layout is before. Since the profile image
  // uses transparency, visually this does not cause any change in cases where
  // the bug doesn't manifest.
  auto* image = views::AsViewClass<views::ImageView>(image_container_view());
  CHECK(image);
  image->SetHorizontalAlignment(views::ImageView::Alignment::kLeading);
  image->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  gfx::Size image_size = image->GetImage().size();
  image_size.Enlarge(1, 1);
  image->SetSize(image_size);
}

bool ManagementToolbarButton::ShouldPaintBorder() const {
  return false;
}

std::optional<SkColor> ManagementToolbarButton::GetHighlightTextColor() const {
  const auto* const color_provider = GetColorProvider();
  CHECK(color_provider);
  return color_provider->GetColor(kColorAvatarButtonHighlightNormalForeground);
}

std::optional<SkColor> ManagementToolbarButton::GetHighlightBorderColor()
    const {
  const auto* const color_provider = GetColorProvider();
  CHECK(color_provider);
  return color_provider->GetColor(kColorToolbarButtonBorder);
}

void ManagementToolbarButton::UpdateText() {
  if (const auto* const color_provider = GetColorProvider();
      color_provider && IsLabelPresentAndVisible()) {
    SetHighlight(/*highlight_text=*/management_label_,
                 /*highlight_color=*/color_provider->GetColor(
                     ui::kColorSysTonalContainer));
  } else {
    SetHighlight(/*highlight_text=*/management_label_,
                 /*highlight_color=*/std::nullopt);
  }
  SetTooltipText(l10n_util::GetStringUTF16(IDS_MANAGED));
  UpdateLayoutInsets();

  // TODO(crbug.com/40689215): this is a hack because toolbar buttons don't
  // correctly calculate their preferred size until they've been laid out once
  // or twice, because they modify their own borders and insets in response to
  // their size and have their own preferred size caching mechanic. These should
  // both ideally be handled with a modern layout manager instead.
  //
  // In the meantime, to ensure that correct (or nearly correct) bounds are set,
  // we will force a resize then invalidate layout to let the layout manager
  // take over.
  SizeToPreferredSize();
  InvalidateLayout();
}

void ManagementToolbarButton::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  UpdateText();
  UpdateIcon();
}

void ManagementToolbarButton::ButtonPressed() {
  base::RecordAction(base::UserMetricsAction(
      "ManagementBubble_OpenedFromManagementToolbarButton"));
  browser_->window()->ShowBubbleFromManagementToolbarButton();
}

ui::ImageModel ManagementToolbarButton::GetIcon() const {
  const int icon_size = ui::TouchUiController::Get()->touch_ui()
                            ? kDefaultTouchableIconSize
                            : kDefaultIconSizeChromeRefresh;
  if (management_icon_.IsEmpty()) {
    return ui::ImageModel::FromVectorIcon(vector_icons::kBusinessIcon,
                                          ui::kColorMenuIcon, icon_size);
  }

  gfx::Image image = profiles::GetSizedAvatarIcon(
      management_icon_, icon_size, icon_size, profiles::SHAPE_SQUARE);
  return ui::ImageModel::FromImageSkia(image.AsImageSkia());
}

bool ManagementToolbarButton::IsLabelPresentAndVisible() const {
  if (!label()) {
    return false;
  }
  return label()->GetVisible() && !label()->GetText().empty();
}

void ManagementToolbarButton::UpdateLayoutInsets() {
  SetLayoutInsets(::GetLayoutInsets(
      IsLabelPresentAndVisible() ? AVATAR_CHIP_PADDING : TOOLBAR_BUTTON));
}

void ManagementToolbarButton::SetManagementLabel(
    const std::string& management_label) {
  management_label_ = base::UTF8ToUTF16(management_label);
  UpdateText();
  UpdateIcon();
}

void ManagementToolbarButton::SetManagementIcon(
    const gfx::Image& management_icon) {
  management_icon_ = management_icon;
  UpdateText();
  UpdateIcon();
}

BEGIN_METADATA(ManagementToolbarButton)
END_METADATA
