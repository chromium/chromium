// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_preconditions.h"

#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/impl/common_preconditions.h"
#include "components/user_education/common/feature_promo/impl/precondition_data.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/user_education/webui/tracked_element_webui.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

DEFINE_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kWindowActivePrecondition);

WindowActivePrecondition::WindowActivePrecondition()
    : FeaturePromoPreconditionBase(kWindowActivePrecondition,
                                   "Target window is active") {}
WindowActivePrecondition::~WindowActivePrecondition() = default;

user_education::FeaturePromoResult WindowActivePrecondition::CheckPrecondition(
    ComputedData& data) const {
  auto& element_ref =
      data.Get(user_education::AnchorElementPrecondition::kAnchorElement);
  views::Widget* widget = nullptr;
  if (auto* const view_el = element_ref.get_as<views::TrackedElementViews>()) {
    widget = view_el->view()->GetWidget();
  } else if (auto* web_el =
                 element_ref.get_as<user_education::TrackedElementWebUI>()) {
    auto* const contents = web_el->handler()->GetWebContents();
    widget = views::Widget::GetWidgetForNativeWindow(
        contents->GetTopLevelNativeWindow());
  }
  return widget && widget->GetPrimaryWindowWidget()->ShouldPaintAsActive()
             ? user_education::FeaturePromoResult::Success()
             : user_education::FeaturePromoResult::kBlockedByUi;
}
