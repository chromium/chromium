// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_VIEWS_CUSTOM_HELP_BUBBLE_VIEW_H_
#define COMPONENTS_USER_EDUCATION_VIEWS_CUSTOM_HELP_BUBBLE_VIEW_H_

#include <concepts>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/help_bubble/custom_help_bubble.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/views/help_bubble_views.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace user_education {

template <typename T>
concept IsCustomHelpBubbleView =
    std::derived_from<T, views::BubbleDialogDelegateView> &&
    std::derived_from<T, CustomHelpBubbleUi>;

// Factory method that generates a custom help bubble view from the home context
// of the promo controller and the build params.
template <typename T>
  requires IsCustomHelpBubbleView<T>
using CustomHelpBubbleViewFactoryCallback =
    base::RepeatingCallback<std::unique_ptr<T>(
        ui::ElementContext from_context,
        HelpBubbleArrow arrow,
        FeaturePromoSpecification::BuildHelpBubbleParams build_params)>;

// Convenience method that creates the callback to feed to
// `FeaturePromoSpecification::CreateForCustomUi()` from a callback that creates
// just the bubble. The bubble will be created with the simpler callback
// provided, shown, and then wrapped in a `CustomHelpBubbleViews` and returned.
//
// If your dialog has default dialog "Accept" and/or "Cancel" buttons, you
// should specify the corresponding `accept_button_action` and
// `cancel_button_action`. The "close" button - i.e. (X) - and the ESC key are
// always automatically handled for you, and you should NEVER call
// `Widget::MakeCloseSynchronous` yourself.
//
// Prefer this to constructing CustomHelpBubbleViews directly.
template <typename T>
  requires IsCustomHelpBubbleView<T>
FeaturePromoSpecification::CustomHelpBubbleFactoryCallback<
    CustomHelpBubbleViews>
CreateCustomHelpBubbleViewFactoryCallback(
    CustomHelpBubbleViewFactoryCallback<T> bubble_factory_callback,
    std::optional<CustomHelpBubbleUi::UserAction> accept_button_action =
        std::nullopt,
    std::optional<CustomHelpBubbleUi::UserAction> cancel_button_action =
        std::nullopt) {
  return base::BindRepeating(
      [](const CustomHelpBubbleViewFactoryCallback<T>& bubble_factory_callback,
         std::optional<CustomHelpBubbleUi::UserAction> accept_button_action,
         std::optional<CustomHelpBubbleUi::UserAction> cancel_button_action,
         ui::ElementContext from_context, HelpBubbleArrow arrow,
         FeaturePromoSpecification::BuildHelpBubbleParams build_params) {
        auto* const anchor_element = build_params.anchor_element.get();
        auto bubble = bubble_factory_callback.Run(from_context, arrow,
                                                  std::move(build_params));
        auto* const bubble_ptr = bubble.get();
        auto widget =
            base::WrapUnique(views::BubbleDialogDelegateView::CreateBubble(
                std::move(bubble),
                views::Widget::InitParams::CLIENT_OWNS_WIDGET));
        widget->Show();
        return std::make_unique<CustomHelpBubbleViews>(
            std::move(widget), bubble_ptr, anchor_element, accept_button_action,
            cancel_button_action);
      },
      std::move(bubble_factory_callback), accept_button_action,
      cancel_button_action);
}

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_VIEWS_CUSTOM_HELP_BUBBLE_VIEW_H_
