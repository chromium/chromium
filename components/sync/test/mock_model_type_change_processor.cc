// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/mock_model_type_change_processor.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/test/forwarding_model_type_change_processor.h"

namespace syncer {

using testing::_;
using testing::Invoke;

MockModelTypeChangeProcessor::MockModelTypeChangeProcessor() = default;

MockModelTypeChangeProcessor::~MockModelTypeChangeProcessor() = default;

std::unique_ptr<ModelTypeChangeProcessor>
MockModelTypeChangeProcessor::CreateForwardingProcessor() {
  return base::WrapUnique<ModelTypeChangeProcessor>(
      new ForwardingModelTypeChangeProcessor(this));
}

void MockModelTypeChangeProcessor::DelegateCallsByDefaultTo(
    ModelTypeChangeProcessor* delegate) {
  DCHECK(delegate);

  ON_CALL(*this, Put)
      .WillByDefault([delegate](const std::string& storage_key,
                                std::unique_ptr<EntityData> entity_data,
                                MetadataChangeList* metadata_change_list) {
        delegate->Put(storage_key, std::move(entity_data),
                      metadata_change_list);
      });
  ON_CALL(*this, Delete)
      .WillByDefault(Invoke(delegate, &ModelTypeChangeProcessor::Delete));
  ON_CALL(*this, UpdateStorageKey)
      .WillByDefault(
          Invoke(delegate, &ModelTypeChangeProcessor::UpdateStorageKey));
  ON_CALL(*this, UntrackEntityForStorageKey)
      .WillByDefault(Invoke(
          delegate, &ModelTypeChangeProcessor::UntrackEntityForStorageKey));
  ON_CALL(*this, UntrackEntityForClientTagHash)
      .WillByDefault(Invoke(
          delegate, &ModelTypeChangeProcessor::UntrackEntityForClientTagHash));
  ON_CALL(*this, IsEntityUnsynced)
      .WillByDefault(
          Invoke(delegate, &ModelTypeChangeProcessor::IsEntityUnsynced));
  ON_CALL(*this, OnModelStarting)
      .WillByDefault(
          Invoke(delegate, &ModelTypeChangeProcessor::OnModelStarting));
  ON_CALL(*this, ModelReadyToSync)
      .WillByDefault([delegate](std::unique_ptr<MetadataBatch> batch) {
        delegate->ModelReadyToSync(std::move(batch));
      });
  ON_CALL(*this, IsTrackingMetadata())
      .WillByDefault(
          Invoke(delegate, &ModelTypeChangeProcessor::IsTrackingMetadata));
  ON_CALL(*this, TrackedAccountId())
      .WillByDefault(
          Invoke(delegate, &ModelTypeChangeProcessor::TrackedAccountId));
  ON_CALL(*this, TrackedCacheGuid())
      .WillByDefault(
          Invoke(delegate, &ModelTypeChangeProcessor::TrackedCacheGuid));
  ON_CALL(*this, ReportError)
      .WillByDefault(Invoke(delegate, &ModelTypeChangeProcessor::ReportError));
  ON_CALL(*this, GetError())
      .WillByDefault(Invoke(delegate, &ModelTypeChangeProcessor::GetError));
  ON_CALL(*this, GetControllerDelegate())
      .WillByDefault(
          Invoke(delegate, &ModelTypeChangeProcessor::GetControllerDelegate));
}

}  //  namespace syncer
