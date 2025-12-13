// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/trace_event/memory_pressure_level_proto.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing_buildflags.h"
#include "components/memory_pressure/system_memory_pressure_evaluator.h"

namespace memory_pressure {
namespace {
BASE_FEATURE(kSuppressMemoryMonitor, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kSuppressMemoryMonitorMask,
                   &kSuppressMemoryMonitor,
                   "suppress_memory_monitor_mask",
                   "");
}  // namespace

MultiSourceMemoryPressureMonitor::MultiSourceMemoryPressureMonitor()
    : current_pressure_level_(base::MEMORY_PRESSURE_LEVEL_NONE),
      dispatch_callback_(base::BindRepeating(
          &base::MemoryPressureListener::NotifyMemoryPressure)),
      aggregator_(this),
      level_reporter_(current_pressure_level_) {}

MultiSourceMemoryPressureMonitor::~MultiSourceMemoryPressureMonitor() {
  // Destroy system evaluator early while the remaining members of this class
  // still exist. MultiSourceMemoryPressureMonitor implements
  // MemoryPressureVoteAggregator::Delegate, and
  // delegate_->OnMemoryPressureLevelChanged() gets indirectly called during
  // ~SystemMemoryPressureEvaluator().
  system_evaluator_.reset();
}

void MultiSourceMemoryPressureMonitor::MaybeStartPlatformVoter() {
  system_evaluator_ =
      SystemMemoryPressureEvaluator::CreateDefaultSystemEvaluator(this);
}

base::MemoryPressureLevel
MultiSourceMemoryPressureMonitor::GetCurrentPressureLevel(
    base::MemoryPressureMonitorTag tag) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(kSuppressMemoryMonitor)) {
    auto mask = kSuppressMemoryMonitorMask.Get();
    const size_t tag_index = static_cast<size_t>(tag);
    // Ignore pressure level only if the caller is suppressed. This is
    // the case if its tag is present in the mask, and if the value is not '0'.
    // A value of '1' suppresses moderate pressure, and a value of '2'
    // supressess moderate and critical levels.
    if (tag_index < mask.size() &&
        (mask[tag_index] == '2' ||
         (mask[tag_index] == '1' &&
          current_pressure_level_ == base::MEMORY_PRESSURE_LEVEL_MODERATE))) {
      return base::MEMORY_PRESSURE_LEVEL_NONE;
    }
  }
  return current_pressure_level_;
}

std::unique_ptr<MemoryPressureVoter>
MultiSourceMemoryPressureMonitor::CreateVoter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return aggregator_.CreateVoter();
}

void MultiSourceMemoryPressureMonitor::OnMemoryPressureLevelChanged(
    base::MemoryPressureLevel level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(current_pressure_level_, level);

  level_reporter_.OnMemoryPressureLevelChanged(level);

  TRACE_EVENT_INSTANT(
      "base", "MultiSourceMemoryPressureMonitor::OnMemoryPressureLevelChanged",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_memory_pressure_notification();
        data->set_level(
            base::trace_event::MemoryPressureLevelToTraceEnum(level));
      });

  current_pressure_level_ = level;
}

void MultiSourceMemoryPressureMonitor::OnNotifyListenersRequested() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  dispatch_callback_.Run(current_pressure_level_);
}

void MultiSourceMemoryPressureMonitor::SetSystemEvaluator(
    std::unique_ptr<SystemMemoryPressureEvaluator> evaluator) {
  DCHECK(!system_evaluator_);
  system_evaluator_ = std::move(evaluator);
}

void MultiSourceMemoryPressureMonitor::SetDispatchCallbackForTesting(
    const DispatchCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Must be called before `Start()`.
  DCHECK(!system_evaluator_);
  dispatch_callback_ = callback;
}

}  // namespace memory_pressure
