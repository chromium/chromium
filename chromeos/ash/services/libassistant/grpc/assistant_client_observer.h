// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace ash::libassistant {

class AssistantClient;

// Observer informed when the |AssistantClient| is created or destroyed.
// This is used internally in our mojom service implementation, to allow our
// different components to know when they can use the Libassistant objects.
class AssistantClientObserver : public base::CheckedObserver {
 public:
  // Called when the |AssistantClient| has been created, but not started yet.
  // The pointers are guaranteed to remain valid until after
  // OnDestroyingAssistantClient() is called.
  virtual void OnAssistantClientCreated(AssistantClient* assistant_client) {}

  // Called when Start() has been called on the |AssistantClient|.  The
  // pointers are guaranteed to remain valid until after
  // OnDestroyingAssistantClient() is called.
  virtual void OnAssistantClientStarted(AssistantClient* assistant_client) {}

  // Called when |AssistantClient| has finished its start logic and is ready
  // to handle queries.
  // The pointers are guaranteed to remain valid until after
  // OnDestroyingAssistantClient() is called.
  virtual void OnAssistantClientRunning(AssistantClient* assistant_client) {}

  // Called just before the |AssistantClient| will be destroyed. They should
  // not be used anymore after this has been called. The pointers passed in are
  // guaranteed to be the same as passed to the last call to
  // OnAssistantClientCreated() (and are just passed in again for the
  // implementer's convenience).
  virtual void OnDestroyingAssistantClient(AssistantClient* assistant_client) {}

  // Called when the |AssistantClient| has been destroyed.
  virtual void OnAssistantClientDestroyed() {}

 protected:
  ~AssistantClientObserver() override = default;
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_OBSERVER_H_
