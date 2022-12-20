// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_CONVERSATION_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_CONVERSATION_OBSERVER_H_

#include "base/component_export.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_interaction_metadata.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"
#include "chromeos/ash/services/libassistant/public/mojom/conversation_observer.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::assistant {

// Default implementation of |mojom::ConversationObserver|, which allow child
// child classes to only implement handlers they are interested in.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC) ConversationObserver
    : public libassistant::mojom::ConversationObserver {
 public:
  // libassistant::mojom::ConversationObserver:
  void OnInteractionStarted(
      const AssistantInteractionMetadata& metadata) override {}
  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override {}
  void OnTtsStarted(bool due_to_error) override {}
  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override {}
  void OnTextResponse(const std::string& response) override {}
  void OnSuggestionsResponse(
      const std::vector<AssistantSuggestion>& suggestions) override {}
  void OnOpenUrlResponse(const GURL& url, bool in_background) override {}
  void OnOpenAppResponse(const AndroidAppInfo& app_info) override {}
  void OnWaitStarted() override {}

  mojo::PendingRemote<libassistant::mojom::ConversationObserver>
  BindNewPipeAndPassRemote();

 protected:
  ConversationObserver();
  ~ConversationObserver() override;

 private:
  mojo::Receiver<libassistant::mojom::ConversationObserver> remote_observer_{
      this};
};

}  // namespace ash::assistant

// TODO(b/258750971): remove when internal assistant codes are migrated to
// namespace ash.
namespace chromeos::assistant {
using ::ash::assistant::ConversationObserver;
}

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_PUBLIC_CPP_CONVERSATION_OBSERVER_H_
