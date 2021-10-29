// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_MODEL_TYPE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/model_type_controller.h"

class PrefService;

namespace history {

class HistoryService;

class TypedURLModelTypeController : public syncer::ModelTypeController {
 public:
  TypedURLModelTypeController(HistoryService* history_service,
                              PrefService* pref_service);

  TypedURLModelTypeController(const TypedURLModelTypeController&) = delete;
  TypedURLModelTypeController& operator=(const TypedURLModelTypeController&) =
      delete;

  ~TypedURLModelTypeController() override;

  // syncer::DataTypeController implementation.
  PreconditionState GetPreconditionState() const override;

 private:
  void OnSavingBrowserHistoryDisabledChanged();

  const raw_ptr<HistoryService> history_service_;
  const raw_ptr<PrefService> pref_service_;

  PrefChangeRegistrar pref_registrar_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_MODEL_TYPE_CONTROLLER_H_
