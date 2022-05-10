// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_MODEL_TYPE_CONTROLLER_H_

#include "components/history/core/browser/sync/history_model_type_controller_helper.h"
#include "components/sync/driver/model_type_controller.h"

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace history {

class HistoryService;

class TypedURLModelTypeController : public syncer::ModelTypeController {
 public:
  TypedURLModelTypeController(syncer::SyncService* sync_service,
                              HistoryService* history_service,
                              PrefService* pref_service);

  TypedURLModelTypeController(const TypedURLModelTypeController&) = delete;
  TypedURLModelTypeController& operator=(const TypedURLModelTypeController&) =
      delete;

  ~TypedURLModelTypeController() override;

  // syncer::DataTypeController implementation.
  PreconditionState GetPreconditionState() const override;

 private:
  HistoryModelTypeControllerHelper helper_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_MODEL_TYPE_CONTROLLER_H_
