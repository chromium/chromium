// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge_test_util.h"

#include <memory>
#include <utility>

#include "components/autofill/core/browser/data_manager/autofill_ai/entity_suppression_sync_bridge.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_local_change_processor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

std::unique_ptr<EntitySuppressionSyncBridge>
CreateTestEntitySuppressionSyncBridge() {
  auto processor =
      std::make_unique<NiceMock<syncer::MockDataTypeLocalChangeProcessor>>();
  ON_CALL(*processor, IsTrackingMetadata()).WillByDefault(Return(true));
  ON_CALL(*processor, GetPossiblyTrimmedRemoteSpecifics)
      .WillByDefault(ReturnRef(sync_pb::EntitySpecifics::default_instance()));
  // The synchronous test `OSCryptAsync` hands its encryptor to the bridge
  // during construction, so it isn't needed afterwards.
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/true);
  return std::make_unique<EntitySuppressionSyncBridge>(
      std::move(processor),
      syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
      os_crypt_async.get());
}

}  // namespace autofill
