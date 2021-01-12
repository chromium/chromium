// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PROXY_CONVERSATION_CONTROLLER_PROXY_H_
#define CHROMEOS_SERVICES_ASSISTANT_PROXY_CONVERSATION_CONTROLLER_PROXY_H_

#include <string>

#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace assistant {

using chromeos::libassistant::mojom::ConversationController;

class ConversationControllerProxy {
 public:
  explicit ConversationControllerProxy(
      mojo::PendingRemote<ConversationController>
          conversation_controller_remote);
  ConversationControllerProxy(const ConversationControllerProxy&) = delete;
  ConversationControllerProxy& operator=(const ConversationControllerProxy&) =
      delete;
  ~ConversationControllerProxy();

  void SendTextQuery(const std::string& query,
                     bool allow_tts,
                     const std::string& conversation_id);

 private:
  mojo::Remote<ConversationController> conversation_controller_remote_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PROXY_CONVERSATION_CONTROLLER_PROXY_H_
