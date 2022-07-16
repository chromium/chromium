// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_url_handler_hover_button.h"

#include <string>
#include <utility>

#include "chrome/browser/ui/views/web_apps/web_app_hover_button.h"
#include "chrome/browser/web_applications/url_handler_launch_params.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/button.h"
#include "url/gurl.h"

WebAppUrlHandlerHoverButton::WebAppUrlHandlerHoverButton(
    views::Button::PressedCallback callback,
    const web_app::UrlHandlerLaunchParams& url_handler_launch_params,
    web_app::WebAppProvider* provider,
    const std::u16string& display_name,
    const GURL& app_start_url)
    : WebAppHoverButton(std::move(callback),
                        url_handler_launch_params.app_id,
                        provider,
                        display_name,
                        app_start_url),
      url_handler_launch_params_(url_handler_launch_params),
      is_app_(true) {}

WebAppUrlHandlerHoverButton::WebAppUrlHandlerHoverButton(
    views::Button::PressedCallback callback)
    : WebAppHoverButton(
          std::move(callback),
          *(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_PRODUCT_LOGO_32)),
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)),
      is_app_(false) {}

WebAppUrlHandlerHoverButton::~WebAppUrlHandlerHoverButton() = default;

void WebAppUrlHandlerHoverButton::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  WebAppHoverButton::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kRadioButton;
  const ax::mojom::CheckedState checked_state =
      selected() ? ax::mojom::CheckedState::kTrue
                 : ax::mojom::CheckedState::kFalse;
  node_data->SetCheckedState(checked_state);
}

void WebAppUrlHandlerHoverButton::MarkAsSelected(const ui::Event* event) {
  WebAppHoverButton::MarkAsSelected(event);
  selected_ = true;
  NotifyAccessibilityEvent(ax::mojom::Event::kStateChanged,
                           /*send_native_event=*/true);
}

void WebAppUrlHandlerHoverButton::MarkAsUnselected(const ui::Event* event) {
  WebAppHoverButton::MarkAsUnselected(event);
  selected_ = false;
  NotifyAccessibilityEvent(ax::mojom::Event::kStateChanged,
                           /*send_native_event=*/true);
}

BEGIN_METADATA(WebAppUrlHandlerHoverButton, WebAppHoverButton)
END_METADATA
