// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/webui/help_bubble_handler.h"

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_auto_reset.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/webui/help_bubble_webui.h"
#include "components/user_education/webui/tracked_element_webui.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom-shared.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"

namespace user_education {

namespace {

// Converts help bubble arrow to WebUI bubble position. This is not a complete
// mapping as many HelpBubbleArrow options are not (yet) supported in WebUI.
help_bubble::mojom::HelpBubblePosition HelpBubbleArrowToPosition(
    HelpBubbleArrow arrow) {
  switch (arrow) {
    case HelpBubbleArrow::kBottomLeft:
    case HelpBubbleArrow::kBottomCenter:
    case HelpBubbleArrow::kBottomRight:
      return help_bubble::mojom::HelpBubblePosition::ABOVE;
    case HelpBubbleArrow::kLeftTop:
    case HelpBubbleArrow::kLeftCenter:
    case HelpBubbleArrow::kLeftBottom:
      return help_bubble::mojom::HelpBubblePosition::RIGHT;
    case HelpBubbleArrow::kRightTop:
    case HelpBubbleArrow::kRightCenter:
    case HelpBubbleArrow::kRightBottom:
      return help_bubble::mojom::HelpBubblePosition::LEFT;
    default:
      return help_bubble::mojom::HelpBubblePosition::BELOW;
  }
}

}  // namespace

HelpBubbleHandlerBase::HelpBubbleHandlerBase(
    std::unique_ptr<ClientProvider> client_provider,
    ui::ElementIdentifier identifier,
    ui::ElementContext context)
    : client_provider_(std::move(client_provider)), context_(context) {
  element_ = std::make_unique<TrackedElementWebUI>(this, identifier, context);
  DCHECK(context_);
}

HelpBubbleHandlerBase::~HelpBubbleHandlerBase() {
  if (current_help_bubble_)
    current_help_bubble_->Close();
}

help_bubble::mojom::HelpBubbleClient* HelpBubbleHandlerBase::GetClient() {
  return client_provider_->GetClient();
}

std::unique_ptr<HelpBubbleWebUI> HelpBubbleHandlerBase::CreateHelpBubble(
    HelpBubbleParams params) {
  DCHECK(!current_help_bubble_params_);
  current_help_bubble_params_ =
      std::make_unique<HelpBubbleParams>(std::move(params));
  auto result = base::WrapUnique(new HelpBubbleWebUI(this));

  auto mojom_params = help_bubble::mojom::HelpBubbleParams::New();
  mojom_params->native_identifier = element_->identifier().GetName();
  mojom_params->body_text =
      base::UTF16ToUTF8(current_help_bubble_params_->body_text);
  mojom_params->position =
      HelpBubbleArrowToPosition(current_help_bubble_params_->arrow);
  if (!current_help_bubble_params_->title_text.empty()) {
    mojom_params->title_text =
        base::UTF16ToUTF8(current_help_bubble_params_->title_text);
  }
  for (auto& button : current_help_bubble_params_->buttons) {
    auto mojom_button = help_bubble::mojom::HelpBubbleButtonParams::New();
    mojom_button->text = base::UTF16ToUTF8(button.text);
    mojom_button->is_default = button.is_default;
    mojom_params->buttons.emplace_back(std::move(mojom_button));
  }

  GetClient()->ShowHelpBubble(std::move(mojom_params));
  current_help_bubble_ = result.get();
  return result;
}

void HelpBubbleHandlerBase::OnHelpBubbleClosing() {
  if (!closing_)
    GetClient()->HideHelpBubble();
  current_help_bubble_ = nullptr;
  current_help_bubble_params_.reset();
}

void HelpBubbleHandlerBase::HelpBubbleHostVisibilityChanged(bool visible) {
  const bool was_visible = element_->visible();
  // Note: any of the following calls could destroy *this* via a callback.
  if (visible && !was_visible) {
    element_->SetVisible(true);
  } else if (!visible && was_visible) {
    // Currently, this is the only call that has follow-up code. If that
    // changes,  the declaration of `weak_ptr` can be moved up to the top of the
    // method.
    auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
    HelpBubbleClosed(false);
    if (!weak_ptr)
      return;
    element_->SetVisible(false);
  }
}

bool HelpBubbleHandlerBase::ToggleHelpBubbleFocusForAccessibility() {
  GetClient()->ToggleFocusForAccessibility();
  return true;
}

gfx::Rect HelpBubbleHandlerBase::GetHelpBubbleBoundsInScreen() const {
  // TODO(dfried): implement.
  return gfx::Rect();
}

void HelpBubbleHandlerBase::HelpBubbleButtonPressed(int8_t button) {
  if (!current_help_bubble_params_)
    return;

  // We can never ensure that objects will persist across callbacks.
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  base::WeakAutoReset reset(weak_ptr, &HelpBubbleHandlerBase::closing_, true);

  base::OnceClosure callback =
      std::move(current_help_bubble_params_->buttons[button].callback);
  if (callback)
    std::move(callback).Run();

  if (weak_ptr && current_help_bubble_)
    current_help_bubble_->Close();
}

void HelpBubbleHandlerBase::HelpBubbleClosed(bool by_user) {
  if (!current_help_bubble_params_)
    return;

  // We can never ensure that `this` will persist across callbacks.
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  base::WeakAutoReset reset(weak_ptr, &HelpBubbleHandlerBase::closing_, true);

  if (by_user) {
    base::OnceClosure callback =
        std::move(current_help_bubble_params_->dismiss_callback);
    if (callback) {
      std::move(callback).Run();
      if (!weak_ptr)
        return;
    }
  }

  if (current_help_bubble_)
    current_help_bubble_->Close();
}

HelpBubbleHandler::ClientProvider::ClientProvider(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> pending_client)
    : remote_client_(std::move(pending_client)) {}

HelpBubbleHandler::ClientProvider::~ClientProvider() = default;

help_bubble::mojom::HelpBubbleClient*
HelpBubbleHandler::ClientProvider::GetClient() {
  return remote_client_.get();
}

HelpBubbleHandler::HelpBubbleHandler(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler>
        pending_handler,
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> pending_client,
    content::WebContents* web_contents,
    ui::ElementIdentifier identifier)
    : HelpBubbleHandlerBase(
          std::make_unique<ClientProvider>(std::move(pending_client)),
          identifier,
          ui::ElementContext(web_contents)),
      receiver_(this, std::move(pending_handler)) {}

HelpBubbleHandler::~HelpBubbleHandler() = default;

}  // namespace user_education
