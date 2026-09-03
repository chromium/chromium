// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEY_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEY_DATA_TYPE_CONTROLLER_H_

#include "components/history/core/browser/sync/history_data_type_controller_helper.h"
#include "components/sync/service/data_type_controller.h"

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace history {

class HistoryService;

// DataTypeController for syncer::JOURNEY.
class JourneyDataTypeController : public syncer::DataTypeController {
 public:
  JourneyDataTypeController(syncer::SyncService* sync_service,
                            HistoryService* history_service,
                            PrefService* pref_service);

  JourneyDataTypeController(const JourneyDataTypeController&) = delete;
  JourneyDataTypeController& operator=(const JourneyDataTypeController&) =
      delete;

  ~JourneyDataTypeController() override;

  // syncer::DataTypeController implementation.
  PreconditionState GetPreconditionState(
      const PreconditionContext& context) const override;

 private:
  HistoryDataTypeControllerHelper helper_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEY_DATA_TYPE_CONTROLLER_H_
