// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/management_toolbar_button.h"

#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
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

// Note that the non-touchable icon size is larger than the default to make the
// management icon easier to read.
constexpr int kIconSizeForNonTouchUi = 22;

bool CanShowManagementToolbarButton(const PrefService& pref_service) {
  return base::FeatureList::IsEnabled(features::kManagementToolbarButton) ||
         !pref_service.GetString(prefs::kEnterpriseCustomLabel).empty() ||
         !pref_service.GetString(prefs::kEnterpriseLogoUrl).empty();
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

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kEnterpriseCustomLabel,
      base::BindRepeating(&ManagementToolbarButton::UpdateManagementInfo,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kEnterpriseLogoUrl,
      base::BindRepeating(&ManagementToolbarButton::UpdateManagementInfo,
                          base::Unretained(this)));
  UpdateManagementInfo();
}

ManagementToolbarButton::~ManagementToolbarButton() = default;

void ManagementToolbarButton::UpdateManagementInfo() {
  PrefService* prefs = profile_->GetPrefs();
  std::string label;
  std::string icon_url;
  bool show_management_toolbar_button = CanShowManagementToolbarButton(*prefs);
  SetVisible(show_management_toolbar_button);
  SetManagementLabel(prefs->GetString(prefs::kEnterpriseCustomLabel));
  if (show_management_toolbar_button) {
    chrome::enterprise_util::GetManagementIcon(
        GURL(prefs->GetString(prefs::kEnterpriseLogoUrl)), profile_,
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

void ManagementToolbarButton::UpdateText() {
  SetTooltipText(l10n_util::GetStringUTF16(IDS_MANAGED));
  SetHighlight(/*text=*/management_label_, /*color=*/std::nullopt);
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
  browser_->window()->ShowBubbleFromManagementToolbarButton();
}

ui::ImageModel ManagementToolbarButton::GetIcon() const {
  static_assert(kIconSizeForNonTouchUi >
                ToolbarButton::kDefaultIconSizeChromeRefresh);
  const int icon_size = ui::TouchUiController::Get()->touch_ui()
                            ? kDefaultIconSizeChromeRefresh
                            : kIconSizeForNonTouchUi;
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
}

void ManagementToolbarButton::SetManagementIcon(
    const gfx::Image& management_icon) {
  management_icon_ = management_icon;
  UpdateIcon();
}

BEGIN_METADATA(ManagementToolbarButton)
END_METADATA
