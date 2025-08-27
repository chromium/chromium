// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_PERSONAL_COLLABORATION_DATA_SERVICE_H_
#define COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_PERSONAL_COLLABORATION_DATA_SERVICE_H_

#include "components/data_sharing/public/personal_collaboration_data/personal_collaboration_data_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace data_sharing {

class MockPersonalCollaborationDataService
    : public personal_collaboration_data::PersonalCollaborationDataService {
 public:
  MockPersonalCollaborationDataService();
  ~MockPersonalCollaborationDataService() override;

  MOCK_METHOD(void, AddObserver, (Observer*));
  MOCK_METHOD(void, RemoveObserver, (Observer*));
  MOCK_METHOD(std::optional<sync_pb::SharedTabGroupAccountDataSpecifics>,
              GetSpecifics,
              (SpecificsType, const std::string&));
  MOCK_METHOD(std::vector<const sync_pb::SharedTabGroupAccountDataSpecifics*>,
              GetAllSpecifics,
              (),
              (const));
  MOCK_METHOD(void,
              CreateOrUpdateSpecifics,
              (SpecificsType,
               const std::string&,
               base::OnceCallback<void(
                   sync_pb::SharedTabGroupAccountDataSpecifics* specifics)>));
  MOCK_METHOD(void, DeleteSpecifics, (SpecificsType, const std::string&));
  MOCK_METHOD(bool, IsInitialized, (), (const));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetControllerDelegate,
              ());
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_TEST_SUPPORT_MOCK_PERSONAL_COLLABORATION_DATA_SERVICE_H_
