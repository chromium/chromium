// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_H_

#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"

namespace data_sharing::personal_collaboration_data {

// The core class for managing personal account linked collaboration data.
class PersonalCollaborationDataService : public KeyedService,
                                         public base::SupportsUserData {
 public:
  ~PersonalCollaborationDataService() override = default;

  // Returns whether the service has fully initialized.
  virtual bool IsInitialized() const = 0;
};

}  // namespace data_sharing::personal_collaboration_data

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_H_
