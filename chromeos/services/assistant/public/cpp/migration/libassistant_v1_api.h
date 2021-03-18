// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_LIBASSISTANT_V1_API_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_LIBASSISTANT_V1_API_H_

#include "base/component_export.h"

namespace assistant_client {
class AssistantManager;
class AssistantManagerInternal;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

// Singleton access to the Libassistant V1 objects.
// Only a single instance of this class may exist at any given time.
// TODO(b/171748795): Remove once all Libassistant access has been moved in the
// //chromeos/services/libassistant service.
class COMPONENT_EXPORT(ASSISTANT_SERVICE_PUBLIC_MIGRATION) LibassistantV1Api {
 public:
  LibassistantV1Api(
      assistant_client::AssistantManager* assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal);
  ~LibassistantV1Api();

  // Return the registered instance. Can be null if no instance was registered.
  static LibassistantV1Api* Get() { return instance_; }

  assistant_client::AssistantManager* assistant_manager() {
    return assistant_manager_;
  }

  assistant_client::AssistantManagerInternal* assistant_manager_internal() {
    return assistant_manager_internal_;
  }

 private:
  static LibassistantV1Api* instance_;
  assistant_client::AssistantManager* const assistant_manager_;
  assistant_client::AssistantManagerInternal* const assistant_manager_internal_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_MIGRATION_LIBASSISTANT_V1_API_H_
