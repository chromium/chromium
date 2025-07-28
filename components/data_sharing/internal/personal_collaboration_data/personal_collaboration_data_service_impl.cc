// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/personal_collaboration_data/personal_collaboration_data_service_impl.h"

namespace data_sharing::personal_collaboration_data {

PersonalCollaborationDataServiceImpl::PersonalCollaborationDataServiceImpl() =
    default;

PersonalCollaborationDataServiceImpl::~PersonalCollaborationDataServiceImpl() =
    default;

bool PersonalCollaborationDataServiceImpl::IsInitialized() const {
  return false;
}

}  // namespace data_sharing::personal_collaboration_data
