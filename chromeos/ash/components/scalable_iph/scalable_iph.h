// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_H_
#define CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace scalable_iph {

// `ScalableIph` provides a scalable way to deliver IPHs.
//
// - Scalable: we provide a scalable way by building this framework on top of
// the feature engagement framework. A developer can set up an IPH without
// modifying a binary. See feature engagement doc for details about its
// flexibility: //components/feature_engagement/README.md.
//
// - IPH: in-product-help.
class ScalableIph : public KeyedService {
 public:
  // List of events ScalableIph supports.
  enum class Event { kFiveMinTick };

  ScalableIph(feature_engagement::Tracker* tracker,
              std::unique_ptr<ScalableIphDelegate> delegate);

  void RecordEvent(Event event);

  ScalableIphDelegate* delegate_for_testing() { return delegate_.get(); }

  // KeyedService:
  ~ScalableIph() override;
  void Shutdown() override;

  void OverrideFeatureListForTesting(
      const std::vector<const base::Feature*> features);
  void OverrideTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  void EnsureTimerStarted();
  void RecordTimeTickEvent();
  void RecordEventInternal(Event event, bool init_success);
  void CheckTriggerConditions();
  const std::vector<const base::Feature*>& GetFeatureList() const;

  raw_ptr<feature_engagement::Tracker> tracker_;
  std::unique_ptr<ScalableIphDelegate> delegate_;
  base::RepeatingTimer timer_;

  std::vector<const base::Feature*> feature_list_for_testing_;

  base::WeakPtrFactory<ScalableIph> weak_ptr_factory_{this};
};

}  // namespace scalable_iph

#endif  // CHROMEOS_ASH_COMPONENTS_SCALABLE_IPH_SCALABLE_IPH_H_
