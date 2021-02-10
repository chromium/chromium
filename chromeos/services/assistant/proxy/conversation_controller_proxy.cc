// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/proxy/conversation_controller_proxy.h"

#include "chromeos/assistant/internal/internal_util.h"

namespace chromeos {
namespace assistant {

ConversationControllerProxy::ConversationControllerProxy(
    mojo::PendingRemote<ConversationController> conversation_controller_remote)
    : conversation_controller_remote_(
          std::move(conversation_controller_remote)) {}

ConversationControllerProxy::~ConversationControllerProxy() = default;

void ConversationControllerProxy::SendTextQuery(
    const std::string& query,
    bool allow_tts,
    const std::string& conversation_id) {
  conversation_controller_remote_->SendTextQuery(query, allow_tts,
                                                 conversation_id);
}

void ConversationControllerProxy::StartEditReminderInteraction(
    const std::string& client_id) {
  conversation_controller_remote_->StartEditReminderInteraction(client_id);
}

void ConversationControllerProxy::RetrieveNotification(
    const AssistantNotification& notification,
    int32_t action_index) {
  conversation_controller_remote_->RetrieveNotification(notification,
                                                        action_index);
}

void ConversationControllerProxy::DismissNotification(
    const AssistantNotification& notification) {
  conversation_controller_remote_->DismissNotification(notification);
}

void ConversationControllerProxy::SendAssistantFeedback(
    const AssistantFeedback& feedback) {
  conversation_controller_remote_->SendAssistantFeedback(feedback);
}

}  // namespace assistant
}  // namespace chromeos
