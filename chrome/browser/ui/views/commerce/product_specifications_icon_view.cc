// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/product_specifications_icon_view.h"

#include "base/metrics/user_metrics.h"
#include "base/timer/timer.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/metrics/metrics_utils.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

ProductSpecificationsIconView::ProductSpecificationsIconView(
    IconLabelBubbleView::Delegate* parent_delegate,
    Delegate* delegate,
    Browser* browser)
    : PageActionIconView(nullptr,
                         0,
                         parent_delegate,
                         delegate,
                         "ProductSpecifications"),
      browser_(browser),
      icon_(&omnibox::kProductSpecificationsAddIcon) {
  SetUpForInOutAnimation();
  SetProperty(views::kElementIdentifierKey,
              kProductSpecificationsChipElementId);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_COMPARE_PAGE_ACTION_ADD_DEFAULT));
}

ProductSpecificationsIconView::~ProductSpecificationsIconView() = default;

views::BubbleDialogDelegate* ProductSpecificationsIconView::GetBubble() const {
  return nullptr;
}

void ProductSpecificationsIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  auto* web_contents = GetWebContents();
  CHECK(web_contents);
  auto* tab_helper = tabs::TabInterface::GetFromContents(web_contents)
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();
  CHECK(tab_helper);

  tab_helper->OnProductSpecificationsIconClicked();

  if (base::FeatureList::IsEnabled(commerce::kProductSpecifications) &&
      base::FeatureList::IsEnabled(commerce::kCompareConfirmationToast)) {
    ShowConfirmationToast(tab_helper->GetComparisonSetName());
  }
}

void ProductSpecificationsIconView::ShowConfirmationToast(
    std::u16string set_name) {
  ToastController* const toast_controller =
      browser_->GetFeatures().toast_controller();
  if (toast_controller) {
    ToastParams params = ToastParams(ToastId::kAddedToComparisonTable);

    params.body_string_replacement_params_ = {set_name};
    toast_controller->MaybeShowToast(ToastParams(std::move(params)));
  }
}

void ProductSpecificationsIconView::ForceVisibleForTesting(bool is_added) {
  SetVisible(true);
  SetVisualState(is_added);
}

const gfx::VectorIcon& ProductSpecificationsIconView::GetVectorIcon() const {
  return *icon_;
}

void ProductSpecificationsIconView::UpdateImpl() {
  bool should_show = ShouldShow();
  if (should_show) {
    // TODO(b/369238920): Delete the SetVisualState after the add to comparison
    // table toast is launched.
    SetVisualState(IsInProductSpecificationsSet());
    MaybeShowPageActionLabel();
  } else {
    HidePageActionLabel();
  }
  SetVisible(should_show);
}

void ProductSpecificationsIconView::AnimationProgressed(
    const gfx::Animation* animation) {
  PageActionIconView::AnimationProgressed(animation);
  // Pause the animation when the label is fully revealed to keep the icon in
  // the expanded state.
  // TODO(crbug.com/40832707): This approach of inspecting the animation
  // progress to extend the animation duration is quite hacky. This should be
  // removed and the IconLabelBubbleView API expanded to support a finer level
  // of control.
  constexpr double kAnimationValueWhenLabelFullyShown = 0.5;
  if (should_extend_label_shown_duration_ &&
      GetAnimationValue() >= kAnimationValueWhenLabelFullyShown) {
    should_extend_label_shown_duration_ = false;
    PauseAnimation();
  }
}

bool ProductSpecificationsIconView::ShouldShow() {
  if (delegate()->ShouldHidePageActionIcons()) {
    return false;
  }
  auto* web_contents = GetWebContents();
  if (!web_contents) {
    return false;
  }
  auto* tab_helper = tabs::TabInterface::GetFromContents(web_contents)
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();

  return tab_helper && tab_helper->ShouldShowProductSpecificationsIconView();
}

void ProductSpecificationsIconView::SetVisualState(bool is_added) {
  icon_ = is_added ? &omnibox::kProductSpecificationsAddedIcon
                   : &omnibox::kProductSpecificationsAddIcon;
  if (GetWebContents()) {
    auto* tab_helper = tabs::TabInterface::GetFromContents(GetWebContents())
                           ->GetTabFeatures()
                           ->commerce_ui_tab_helper();
    CHECK(tab_helper);

    SetLabel(tab_helper->GetProductSpecificationsLabel(is_added));
  }
  SetBackgroundVisibility(BackgroundVisibility::kWithLabel);
  UpdateIconImage();
}

void ProductSpecificationsIconView::MaybeShowPageActionLabel() {
  if (!base::FeatureList::IsEnabled(commerce::kCommerceAllowChipExpansion)) {
    return;
  }
  auto* tab_helper = tabs::TabInterface::GetFromContents(GetWebContents())
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();
  if (!tab_helper || !tab_helper->ShouldExpandPageActionIcon(
                         PageActionIconType::kProductSpecifications)) {
    return;
  }
  should_extend_label_shown_duration_ = true;
  AnimateIn(std::nullopt);
}

void ProductSpecificationsIconView::HidePageActionLabel() {
  UnpauseAnimation();
  ResetSlideAnimation(false);
}

bool ProductSpecificationsIconView::IsInProductSpecificationsSet() const {
  if (!GetWebContents()) {
    return false;
  }

  auto* tab_helper = tabs::TabInterface::GetFromContents(GetWebContents())
                         ->GetTabFeatures()
                         ->commerce_ui_tab_helper();
  CHECK(tab_helper);

  return tab_helper->IsInRecommendedSet();
}

BEGIN_METADATA(ProductSpecificationsIconView)
END_METADATA
