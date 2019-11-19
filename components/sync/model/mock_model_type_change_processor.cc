// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/mock_model_type_change_processor.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/sync/model/metadata_batch.h"

namespace syncer {
namespace {

using testing::Invoke;
using testing::_;

class ForwardingModelTypeChangeProcessor : public ModelTypeChangeProcessor {
 public:
  // |other| must not be nullptr and must outlive this object.
  explicit ForwardingModelTypeChangeProcessor(ModelTypeChangeProcessor* other)
      : other_(other) {}
  ~ForwardingModelTypeChangeProcessor() override {}

  void Put(const std::string& client_tag,
           std::unique_ptr<EntityData> entity_data,
           MetadataChangeList* metadata_change_list) override {
    other_->Put(client_tag, std::move(entity_data), metadata_change_list);
  }

  void Delete(const std::string& client_tag,
              MetadataChangeList* metadata_change_list) override {
    other_->Delete(client_tag, metadata_change_list);
  }

  void UpdateStorageKey(const EntityData& entity_data,
                        const std::string& storage_key,
                        MetadataChangeList* metadata_change_list) override {
    other_->UpdateStorageKey(entity_data, storage_key, metadata_change_list);
  }

  void UntrackEntityForStorageKey(const std::string& storage_key) override {
    other_->UntrackEntityForStorageKey(storage_key);
  }

  void UntrackEntityForClientTagHash(
      const ClientTagHash& client_tag_hash) override {
    other_->UntrackEntityForClientTagHash(client_tag_hash);
  }

  bool IsEntityUnsynced(const std::string& storage_key) override {
    return other_->IsEntityUnsynced(storage_key);
  }

  base::Time GetEntityCreationTime(
      const std::string& storage_key) const override {
    return other_->GetEntityCreationTime(storage_key);
  }

  base::Time GetEntityModificationTime(
      const std::string& storage_key) const override {
    return other_->GetEntityModificationTime(storage_key);
  }

  void OnModelStarting(ModelTypeSyncBridge* bridge) override {
    other_->OnModelStarting(bridge);
  }

  void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) override {
    other_->ModelReadyToSync(std::move(batch));
  }

  bool IsTrackingMetadata() override { return other_->IsTrackingMetadata(); }

  std::string TrackedAccountId() override { return other_->TrackedAccountId(); }

  std::string TrackedCacheGuid() override { return other_->TrackedCacheGuid(); }

  void ReportError(const ModelError& error) override {
    other_->ReportError(error);
  }

  base::Optional<ModelError> GetError() const override {
    return other_->GetError();
  }

  base::WeakPtr<ModelTypeControllerDelegate> GetControllerDelegate() override {
    return other_->GetControllerDelegate();
  }

 private:
  ModelTypeChangeProcessor* other_;
};

}  // namespace

MockModelTypeChangeProcessor::MockModelTypeChangeProcessor() {}

MockModelTypeChangeProcessor::~MockModelTypeChangeProcessor() {}

std::unique_ptr<ModelTypeChangeProcessor>
MockModelTypeChangeProcessor::CreateForwardingProcessor() {
  return base::WrapUnique<ModelTypeChangeProcessor>(
      new ForwardingModelTypeChangeProcessor(this));
}

void MockModelTypeChangeProcessor::DelegateCallsByDefaultTo(
    ModelTypeChangeProcessor* delegate) {
  DCHECK(delegate);

  ON_CALL(*this, Put(_, _, _))
      .WillByDefault([delegate](const std::string& storage_key,
                                std::unique_ptr<EntityData> entity_data,
                                MetadataChangeList* metadata_change_list) {
        delegate->Put(storage_key, std::move(entity_data),
                      metadata_change_list);
      });
  ON_CALL(*this, Delete(_, _))
      .WillByDefault(Invoke(delegate, &ModelTypeChangeProcessor::Delete));
  ON_CALL(*this, UpdateStorageKey(_, _, _))
      .WillByDefault(
          Invoke(delegate, &ModelTypeChangeProcessor::UpdateStorageKey));
  ON_CALL(*this, UntrackEntityForStorageKey(_))
      .WillByDefault(Invoke(
          delegate, &ModelTypeChangeProcessor::UntrackEntityForStorageKey));
  ON_CALL(*this, UntrackEntityForClientTagHash(_))
      .WillByDefault(Invoke(
          delegate, &ModelTypeChangeProcessor::UntrackEntityForClientTagHash));
  ON_CALL(*this, IsEntityUnsynced(_))
      .WillByDefault(
          Invoke(delegate, &ModelTypeChangeProcessor::IsEntityUnsynced));
  ON_CALL(*this, OnModelStarting(_))
      .WillByDefault(
          Invoke(delegate, &ModelTypeChangeProcessor::OnModelStarting));
  ON_CALL(*this, ModelReadyToSync(_))
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
  ON_CALL(*this, ReportError(_))
      .WillByDefault(Invoke(delegate, &ModelTypeChangeProcessor::ReportError));
  ON_CALL(*this, GetError())
      .WillByDefault(Invoke(delegate, &ModelTypeChangeProcessor::GetError));
  ON_CALL(*this, GetControllerDelegate())
      .WillByDefault(
          Invoke(delegate, &ModelTypeChangeProcessor::GetControllerDelegate));
}

}  //  namespace syncer
