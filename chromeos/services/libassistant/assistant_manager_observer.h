// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_ASSISTANT_MANAGER_OBSERVER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_ASSISTANT_MANAGER_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
}  // namespace assistant_client

namespace chromeos {
namespace libassistant {

// Observer informed when the |AssistantManager| is created or destroyed.
// This is used internally in our mojom service implementation, to allow our
// different components to know when they can use the Libassistant V1 objects.
class COMPONENT_EXPORT(LIBASSISTANT_SERVICE) AssistantManagerObserver
    : public base::CheckedObserver {
 public:
  ~AssistantManagerObserver() override = default;

  // Called when the |AssistantManager| and |AssistantManagerInternal| have
  // been created, but not started yet.
  // The pointers are guaranteed to remain valid until after
  // OnDestroyingAssistantManager() is called.
  virtual void OnAssistantManagerCreated(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal) {}

  // Called when Start() has been called on the |AssistantManager|.
  // The pointers are guaranteed to remain valid until after
  // OnDestroyingAssistantManager() is called.
  virtual void OnAssistantManagerStarted(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal) {}

  // Called when |AssistantManager| has finished its start logic and is ready
  // to handle queries.
  // The pointers are guaranteed to remain valid until after
  // OnDestroyingAssistantManager() is called.
  virtual void OnAssistantManagerRunning(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal) {}

  // Called just before the |AssistantManager| and |AssistantManagerInternal|
  // will be destroyed. They should not be used anymore after this has been
  // called. The pointers passed in are guaranteed to be the same as passed to
  // the last call to OnAssistantManagerCreated() (and are just passed in again
  // for the implementer's convenience).
  virtual void OnDestroyingAssistantManager(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal) {}

  // Called when the |AssistantManager| and |AssistantManagerInternal| have
  // been destroyed.
  virtual void OnAssistantManagerDestroyed() {}
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_ASSISTANT_MANAGER_OBSERVER_H_
