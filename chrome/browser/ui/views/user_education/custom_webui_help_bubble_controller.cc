// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/custom_webui_help_bubble_controller.h"

#include "base/memory/raw_ref.h"
#include "components/user_education/common/help_bubble/custom_help_bubble.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/resources/cr_components/help_bubble/custom_help_bubble.mojom.h"

// Implements the CustomHelpBubbleHandler and does translation between mojom and
// CustomHelpBubble* events.
class CustomWebUIHelpBubbleController::CustomHelpBubbleHandler
    : public custom_help_bubble::mojom::CustomHelpBubbleHandler {
 public:
  CustomHelpBubbleHandler(
      CustomWebUIHelpBubbleController& controller,
      mojo::PendingReceiver<custom_help_bubble::mojom::CustomHelpBubbleHandler>
          pending_receiver)
      : controller_(controller), receiver_(this, std::move(pending_receiver)) {}

  using MojomUserAction = custom_help_bubble::mojom::CustomHelpBubbleUserAction;
  using UiUserAction = CustomHelpBubbleUi::UserAction;

  // custom_help_bubble::mojom::CustomHelpBubbleHandler:
  void NotifyUserAction(MojomUserAction action) override {
    UiUserAction ui_action;
    switch (action) {
      case MojomUserAction::kAction:
        ui_action = UiUserAction::kAction;
        break;
      case MojomUserAction::kCancel:
        ui_action = UiUserAction::kCancel;
        break;
      case MojomUserAction::kDismiss:
        ui_action = UiUserAction::kDismiss;
        break;
      case MojomUserAction::kSnooze:
        ui_action = UiUserAction::kSnooze;
        break;
    }
    controller_->NotifyUserAction(ui_action);
  }

 private:
  const raw_ref<CustomWebUIHelpBubbleController> controller_;
  mojo::Receiver<custom_help_bubble::mojom::CustomHelpBubbleHandler> receiver_;
};

CustomWebUIHelpBubbleController::CustomWebUIHelpBubbleController() = default;
CustomWebUIHelpBubbleController::~CustomWebUIHelpBubbleController() = default;

void CustomWebUIHelpBubbleController::BindInterface(
    mojo::PendingReceiver<
        custom_help_bubble::mojom::CustomHelpBubbleHandlerFactory>
        pending_receiver) {
  if (custom_help_bubble_handler_factory_receiver_.is_bound()) {
    custom_help_bubble_handler_factory_receiver_.reset();
  }
  custom_help_bubble_handler_factory_receiver_.Bind(
      std::move(pending_receiver));
}

void CustomWebUIHelpBubbleController::CreateCustomHelpBubbleHandler(
    mojo::PendingReceiver<custom_help_bubble::mojom::CustomHelpBubbleHandler>
        handler) {
  custom_help_bubble_handler_ =
      std::make_unique<CustomHelpBubbleHandler>(*this, std::move(handler));
}
