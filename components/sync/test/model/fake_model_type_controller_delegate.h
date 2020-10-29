// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_MODEL_FAKE_MODEL_TYPE_CONTROLLER_DELEGATE_H_
#define COMPONENTS_SYNC_TEST_MODEL_FAKE_MODEL_TYPE_CONTROLLER_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace syncer {

class FakeModelTypeControllerDelegate : public ModelTypeControllerDelegate {
 public:
  explicit FakeModelTypeControllerDelegate(ModelType type);
  ~FakeModelTypeControllerDelegate() override;

  // By default, this delegate (model) completes startup automatically when
  // OnSyncStarting() is invoked. For tests that want to manually control the
  // completion, or mimic errors during startup, EnableManualModelStart() can
  // be used, which means OnSyncStarting() will only complete when
  // SimulateModelStartFinished() is called.
  void EnableManualModelStart();
  void SimulateModelStartFinished();

  // Simulates a model running into an error (e.g. IO failed). This can happen
  // before or after the model has started/loaded.
  void SimulateModelError(const ModelError& error);

  // The number of times OnSyncStopping() was called with CLEAR_METADATA.
  int clear_metadata_call_count() const;

  // ModelTypeControllerDelegate overrides
  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override;
  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void RecordMemoryUsageAndCountsHistograms() override;
  void GetTypeEntitiesCountForDebugging(
      base::OnceCallback<void(const TypeEntitiesCount&)> callback)
      const override;

  base::WeakPtr<ModelTypeControllerDelegate> GetWeakPtr();

 private:
  const ModelType type_;
  bool manual_model_start_enabled_ = false;
  int clear_metadata_call_count_ = 0;
  base::Optional<ModelError> model_error_;
  StartCallback start_callback_;
  ModelErrorHandler error_handler_;
  base::WeakPtrFactory<FakeModelTypeControllerDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeModelTypeControllerDelegate);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_MODEL_FAKE_MODEL_TYPE_CONTROLLER_DELEGATE_H_
