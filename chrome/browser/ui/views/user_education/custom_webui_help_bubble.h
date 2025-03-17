// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_CUSTOM_WEBUI_HELP_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_CUSTOM_WEBUI_HELP_BUBBLE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_dialog_view.h"
#include "chrome/browser/ui/views/user_education/custom_webui_help_bubble_controller.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/views/help_bubble_views.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "url/gurl.h"

// Class that wraps a WebUI whose controller is a `CustomHelpBubbleUi` in a
// dialog and shows it as a help bubble.
//
// Most of the time you will want to use the function
// `MakeCustomWebUIHelpBubbleFactoryCallback()` below to tell the User Education
// system how to create your custom IPH UI, rather than using this class
// directly.
class CustomWebUIHelpBubble : public user_education::CustomHelpBubbleViews {
 public:
  ~CustomWebUIHelpBubble() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kHelpBubbleIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kWebViewIdForTesting);

  // Most WebUI custom help bubbles should be fine using a vanilla
  // `WebUIBubbleDialogView`, but this allows you to override that with your own
  // derived class when constructing a custom WebUI-based help bubble, in case
  // you need to e.g. change preferred size behavior.
  using BuildCustomWebUIHelpBubbleViewCallback =
      base::RepeatingCallback<std::unique_ptr<WebUIBubbleDialogView>(
          views::View* anchor_view,
          views::BubbleBorder::Arrow arrow,
          base::WeakPtr<WebUIContentsWrapper> contents_wrapper)>;
  static BuildCustomWebUIHelpBubbleViewCallback
  GetDefaultBuildBubbleViewCallback();

  // Method for constructing a `CustomWebUIHelpBubble` from a set of parameters.
  // Used by `MakeCustomWebUIHelpBubbleFactoryCallback()`; you probably don't
  // need to call this directly.
  template <typename T>
    requires IsCustomWebUIHelpBubbleController<T>
  static std::unique_ptr<CustomWebUIHelpBubble> CreateForController(
      const GURL& webui_url,
      ui::ElementContext from_context,
      user_education::HelpBubbleArrow arrow,
      user_education::FeaturePromoSpecification::BuildHelpBubbleParams params,
      const BuildCustomWebUIHelpBubbleViewCallback& build_bubble_view_callback =
          GetDefaultBuildBubbleViewCallback());

 protected:
  template <typename T>
    requires IsCustomWebUIHelpBubbleController<T>
  CustomWebUIHelpBubble(std::unique_ptr<views::Widget> widget,
                        WebUIBubbleDialogView* bubble,
                        std::unique_ptr<WebUIContentsWrapperT<T>> wrapper,
                        ui::TrackedElement* anchor_element);

 private:
  std::unique_ptr<WebUIContentsWrapper> wrapper_;
};

template <typename T>
  requires IsCustomWebUIHelpBubbleController<T>
user_education::FeaturePromoSpecification::CustomHelpBubbleFactoryCallback<
    CustomWebUIHelpBubble>
MakeCustomWebUIHelpBubbleFactoryCallback(
    GURL webui_url,
    const CustomWebUIHelpBubble::BuildCustomWebUIHelpBubbleViewCallback&
        build_bubble_view_callback =
            CustomWebUIHelpBubble::GetDefaultBuildBubbleViewCallback()) {
  return base::BindRepeating(
      [](const GURL& webui_url,
         const CustomWebUIHelpBubble::BuildCustomWebUIHelpBubbleViewCallback&
             build_bubble_view_callback,
         ui::ElementContext from_context, user_education::HelpBubbleArrow arrow,
         user_education::FeaturePromoSpecification::BuildHelpBubbleParams
             params) {
        auto bubble = CustomWebUIHelpBubble::CreateForController<T>(
            webui_url, from_context, arrow, std::move(params),
            build_bubble_view_callback);
        return bubble;
      },
      webui_url, build_bubble_view_callback);
}

// Template class member definitions.

template <typename T>
  requires IsCustomWebUIHelpBubbleController<T>
CustomWebUIHelpBubble::CustomWebUIHelpBubble(
    std::unique_ptr<views::Widget> widget,
    WebUIBubbleDialogView* bubble,
    std::unique_ptr<WebUIContentsWrapperT<T>> wrapper,
    ui::TrackedElement* anchor_element)
    : CustomHelpBubbleViews(std::move(widget),
                            bubble,
                            *wrapper->GetWebUIController(),
                            anchor_element,
                            std::nullopt,
                            std::nullopt),
      wrapper_(std::move(wrapper)) {}

// static
template <typename T>
  requires IsCustomWebUIHelpBubbleController<T>
std::unique_ptr<CustomWebUIHelpBubble>
CustomWebUIHelpBubble::CreateForController(
    const GURL& webui_url,
    ui::ElementContext from_context,
    user_education::HelpBubbleArrow arrow,
    user_education::FeaturePromoSpecification::BuildHelpBubbleParams params,
    const BuildCustomWebUIHelpBubbleViewCallback& build_bubble_view_callback) {
  Browser* const browser =
      chrome::FindBrowserWithUiElementContext(from_context);  // NOLINT
  CHECK(browser);
  auto wrapper = std::make_unique<WebUIContentsWrapperT<T>>(
      webui_url, browser->profile(), IDS_HELP_BUBBLE);
  auto ui = wrapper->GetWebUIController()->GetCustomUiAsWeakPtr();
  auto bubble_ptr = build_bubble_view_callback.Run(
      params.anchor_element->AsA<views::TrackedElementViews>()->view(),
      user_education::HelpBubbleViews::TranslateArrow(arrow),
      wrapper->GetWeakPtr());
  auto* const bubble = bubble_ptr.get();
  auto widget = base::WrapUnique(views::BubbleDialogDelegateView::CreateBubble(
      std::move(bubble_ptr), views::Widget::InitParams::CLIENT_OWNS_WIDGET));
  wrapper->ShowUI();
  return base::WrapUnique(new CustomWebUIHelpBubble(
      std::move(widget), bubble, std::move(wrapper), params.anchor_element));
}

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_CUSTOM_WEBUI_HELP_BUBBLE_H_
