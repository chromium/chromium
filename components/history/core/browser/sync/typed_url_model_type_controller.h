// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_MODEL_TYPE_CONTROLLER_H_

#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/model_type_controller.h"

class PrefService;

namespace history {

class HistoryService;

class TypedURLModelTypeController : public syncer::ModelTypeController {
 public:
  TypedURLModelTypeController(HistoryService* history_service,
                              PrefService* pref_service,
                              const char* history_disabled_pref_name);
  ~TypedURLModelTypeController() override;

  // syncer::DataTypeController implementation.
  PreconditionState GetPreconditionState() const override;

 private:
  void OnSavingBrowserHistoryDisabledChanged();

  HistoryService* const history_service_;
  PrefService* const pref_service_;

  // Name of the pref that indicates whether saving history is disabled.
  const char* const history_disabled_pref_name_;

  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(TypedURLModelTypeController);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_MODEL_TYPE_CONTROLLER_H_
