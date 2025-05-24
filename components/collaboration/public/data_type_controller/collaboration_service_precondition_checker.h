// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_DATA_TYPE_CONTROLLER_COLLABORATION_SERVICE_PRECONDITION_CHECKER_H_
#define COMPONENTS_COLLABORATION_PUBLIC_DATA_TYPE_CONTROLLER_COLLABORATION_SERVICE_PRECONDITION_CHECKER_H_

#include "base/functional/callback_forward.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/sync/service/data_type_controller.h"

namespace collaboration {

// Helper class to determine whether a user is eligible for syncing the
// SHARED_TAB_GROUP_DATA and COLLABORATION_GROUP sync data types. This is needed
// to disable the data type for unsupported users such as enterprise users in
// the data type controller.
class CollaborationServicePreconditionChecker
    : public collaboration::CollaborationService::Observer {
 public:
  // `on_precondition_changed` is called whenever the result of
  // `GetPreconditionState()` has possibly changed.
  CollaborationServicePreconditionChecker(
      collaboration::CollaborationService* collaboration_service,
      base::RepeatingClosure on_precondition_changed);
  ~CollaborationServicePreconditionChecker() override;

  syncer::DataTypeController::PreconditionState GetPreconditionState() const;

  // CollaborationService::Observer overrides.
  void OnServiceStatusChanged(
      const collaboration::CollaborationService::Observer::ServiceStatusUpdate&
          update) override;

 private:
  const raw_ref<collaboration::CollaborationService> collaboration_service_;
  const base::RepeatingClosure on_precondition_changed_;
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_PUBLIC_DATA_TYPE_CONTROLLER_COLLABORATION_SERVICE_PRECONDITION_CHECKER_H_
