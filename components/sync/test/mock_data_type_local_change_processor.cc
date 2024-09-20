// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/mock_data_type_local_change_processor.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/test/forwarding_data_type_local_change_processor.h"

namespace syncer {

using testing::_;
using testing::Invoke;

MockDataTypeLocalChangeProcessor::MockDataTypeLocalChangeProcessor() = default;

MockDataTypeLocalChangeProcessor::~MockDataTypeLocalChangeProcessor() = default;

base::WeakPtr<DataTypeLocalChangeProcessor>
MockDataTypeLocalChangeProcessor::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<DataTypeLocalChangeProcessor>
MockDataTypeLocalChangeProcessor::CreateForwardingProcessor() {
  return base::WrapUnique<DataTypeLocalChangeProcessor>(
      new ForwardingDataTypeLocalChangeProcessor(this));
}

void MockDataTypeLocalChangeProcessor::DelegateCallsByDefaultTo(
    DataTypeLocalChangeProcessor* delegate) {
  DCHECK(delegate);

  ON_CALL(*this, Put)
      .WillByDefault([delegate](const std::string& storage_key,
                                std::unique_ptr<EntityData> entity_data,
                                MetadataChangeList* metadata_change_list) {
        delegate->Put(storage_key, std::move(entity_data),
                      metadata_change_list);
      });
  // Lambda is used instead of testing::Invoke() due to overloads.
  ON_CALL(*this, Delete)
      .WillByDefault([delegate](const std::string& storage_key,
                                const DeletionOrigin& origin,
                                MetadataChangeList* metadata_change_list) {
        delegate->Delete(storage_key, origin, metadata_change_list);
      });
  ON_CALL(*this, UpdateStorageKey)
      .WillByDefault(
          Invoke(delegate, &DataTypeLocalChangeProcessor::UpdateStorageKey));
  ON_CALL(*this, UntrackEntityForStorageKey)
      .WillByDefault(Invoke(
          delegate, &DataTypeLocalChangeProcessor::UntrackEntityForStorageKey));
  ON_CALL(*this, UntrackEntityForClientTagHash)
      .WillByDefault(
          Invoke(delegate,
                 &DataTypeLocalChangeProcessor::UntrackEntityForClientTagHash));
  ON_CALL(*this, IsEntityUnsynced)
      .WillByDefault(
          Invoke(delegate, &DataTypeLocalChangeProcessor::IsEntityUnsynced));
  ON_CALL(*this, OnModelStarting)
      .WillByDefault(
          Invoke(delegate, &DataTypeLocalChangeProcessor::OnModelStarting));
  ON_CALL(*this, ModelReadyToSync)
      .WillByDefault([delegate](std::unique_ptr<MetadataBatch> batch) {
        delegate->ModelReadyToSync(std::move(batch));
      });
  ON_CALL(*this, IsTrackingMetadata())
      .WillByDefault(
          Invoke(delegate, &DataTypeLocalChangeProcessor::IsTrackingMetadata));
  ON_CALL(*this, TrackedAccountId())
      .WillByDefault(
          Invoke(delegate, &DataTypeLocalChangeProcessor::TrackedAccountId));
  ON_CALL(*this, TrackedCacheGuid())
      .WillByDefault(
          Invoke(delegate, &DataTypeLocalChangeProcessor::TrackedCacheGuid));
  ON_CALL(*this, ReportError)
      .WillByDefault(
          Invoke(delegate, &DataTypeLocalChangeProcessor::ReportError));
  ON_CALL(*this, GetError())
      .WillByDefault(Invoke(delegate, &DataTypeLocalChangeProcessor::GetError));
  ON_CALL(*this, GetControllerDelegate())
      .WillByDefault(Invoke(
          delegate, &DataTypeLocalChangeProcessor::GetControllerDelegate));
}

}  //  namespace syncer
