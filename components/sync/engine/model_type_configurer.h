// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_MODEL_TYPE_CONFIGURER_H_
#define COMPONENTS_SYNC_ENGINE_MODEL_TYPE_CONFIGURER_H_

#include <memory>

#include "base/callback.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/configure_reason.h"

namespace syncer {

struct DataTypeActivationResponse;

// The DataTypeConfigurer interface abstracts out the action of
// configuring a set of new data types and cleaning up after a set of
// removed data types.
class ModelTypeConfigurer {
 public:
  // Utility struct for holding ConfigureDataTypes options.
  struct ConfigureParams {
    ConfigureParams();
    ConfigureParams(ConfigureParams&& other);
    ~ConfigureParams();
    ConfigureParams& operator=(ConfigureParams&& other);

    ConfigureReason reason;
    ModelTypeSet to_download;
    ModelTypeSet to_purge;

    base::OnceCallback<void(ModelTypeSet succeeded, ModelTypeSet failed)>
        ready_task;

    // Whether full sync (or sync the feature) is enabled;
    bool is_sync_feature_enabled;

   private:
    DISALLOW_COPY_AND_ASSIGN(ConfigureParams);
  };

  ModelTypeConfigurer();
  virtual ~ModelTypeConfigurer();

  // Changes the set of data types that are currently being synced.
  virtual void ConfigureDataTypes(ConfigureParams params) = 0;

  // Activates change processing for the given data type.
  // This must be called before initial sync for data type.
  virtual void ActivateDataType(
      ModelType type,
      std::unique_ptr<DataTypeActivationResponse> activation_response) = 0;

  // Deactivates change processing for the given data type.
  virtual void DeactivateDataType(ModelType type) = 0;

  // Activates a proxy type, which determines whether protocol fields such as
  // |tabs_datatype_enabled| should be true.
  virtual void ActivateProxyDataType(ModelType type) = 0;

  // Deactivates a proxy type.
  virtual void DeactivateProxyDataType(ModelType type) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_MODEL_TYPE_CONFIGURER_H_
