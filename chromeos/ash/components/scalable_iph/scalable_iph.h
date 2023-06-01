// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_H_

#include "base/memory/weak_ptr.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace scalable_iph {

class ScalableIph : public KeyedService {
 public:
  // List of events ScalableIph supports.
  enum Event { kFiveMinTick };

  explicit ScalableIph(feature_engagement::Tracker* tracker);

  void RecordEvent(Event event);

  // KeyedService:
  ~ScalableIph() override;
  void Shutdown() override;

 private:
  void RecordEventInternal(Event event, bool init_success);
  void CheckTriggerConditions();
  void TriggerIph(const base::Feature& feature);

  raw_ptr<feature_engagement::Tracker> tracker_;

  base::WeakPtrFactory<ScalableIph> weak_ptr_factory_{this};
};

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_H_
