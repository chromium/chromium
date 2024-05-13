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
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
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
  SetProperty(views::kElementIdentifierKey,
              kProductSpecificationsChipElementId);
}

ProductSpecificationsIconView::~ProductSpecificationsIconView() = default;

views::BubbleDialogDelegate* ProductSpecificationsIconView::GetBubble() const {
  return nullptr;
}

void ProductSpecificationsIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  NOTIMPLEMENTED();
  // TODO(b/325660810): Add implementation for execution.
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
  SetLabel(l10n_util::GetStringUTF16(
      IDS_PRODUCT_SPECIFICATIONS_PAGE_ACTION_ADD_DEFAULT));
  SetVisible(should_show);
}

bool ProductSpecificationsIconView::ShouldShow() {
  // TODO(b/325660810): Add implementation to decide if icon should show.
  return false;
}

void ProductSpecificationsIconView::SetVisualState(bool is_added) {
  icon_ = is_added ? &omnibox::kProductSpecificationsAddedIcon
                   : &omnibox::kProductSpecificationsAddIcon;

  SetPaintLabelOverSolidBackground(true);
  UpdateIconImage();
}

BEGIN_METADATA(ProductSpecificationsIconView)
END_METADATA
