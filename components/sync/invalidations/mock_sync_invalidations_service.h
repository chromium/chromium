// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_MOCK_SYNC_INVALIDATIONS_SERVICE_H_
#define COMPONENTS_SYNC_INVALIDATIONS_MOCK_SYNC_INVALIDATIONS_SERVICE_H_

#include <string>

#include "components/sync/invalidations/sync_invalidations_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class MockSyncInvalidationsService : public SyncInvalidationsService {
 public:
  MockSyncInvalidationsService();
  ~MockSyncInvalidationsService() override;

  MOCK_METHOD(void, SetActive, (bool active));
  MOCK_METHOD(void, AddListener, (InvalidationsListener * listener));
  MOCK_METHOD(void, RemoveListener, (InvalidationsListener * listener));
  MOCK_METHOD(void,
              AddTokenObserver,
              (FCMRegistrationTokenObserver * observer));
  MOCK_METHOD(void,
              RemoveTokenObserver,
              (FCMRegistrationTokenObserver * observer));
  MOCK_METHOD(const std::string&, GetFCMRegistrationToken, (), (const));
  MOCK_METHOD(void,
              SetInterestedDataTypesHandler,
              (InterestedDataTypesHandler * handler));
  MOCK_METHOD(const ModelTypeSet&, GetInterestedDataTypes, (), (const));
  MOCK_METHOD(void,
              SetInterestedDataTypes,
              (const ModelTypeSet& data_types,
               InterestedDataTypesAppliedCallback callback));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_MOCK_SYNC_INVALIDATIONS_SERVICE_H_
