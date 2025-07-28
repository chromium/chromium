// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_IMPL_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_IMPL_H_

#include "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"

namespace data_sharing::personal_collaboration_data {

// The core class for managing personal account linked collaboration data.
class PersonalCollaborationDataServiceImpl
    : public PersonalCollaborationDataService {
 public:
  PersonalCollaborationDataServiceImpl();
  ~PersonalCollaborationDataServiceImpl() override;

  // Disallow copy/assign.
  PersonalCollaborationDataServiceImpl(
      const PersonalCollaborationDataServiceImpl&) = delete;
  PersonalCollaborationDataServiceImpl& operator=(
      const PersonalCollaborationDataServiceImpl&) = delete;

  // PersonalCollaborationDataService implementation.
  bool IsInitialized() const override;
};

}  // namespace data_sharing::personal_collaboration_data

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_PERSONAL_COLLABORATION_DATA_PERSONAL_COLLABORATION_DATA_SERVICE_IMPL_H_
