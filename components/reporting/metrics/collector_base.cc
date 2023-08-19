// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/collector_base.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/metrics/sampler.h"

namespace reporting {

CollectorBase::CollectorBase(Sampler* sampler) : sampler_(sampler) {}

CollectorBase::~CollectorBase() {
  CheckOnSequence();
}

void CollectorBase::Collect(bool is_event_driven) {
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  CheckOnSequence();

  if (!CanCollect()) {
    return;
  }

  auto on_collected_cb =
      base::BindOnce(&CollectorBase::OnMetricDataCollected,
                     weak_ptr_factory_.GetWeakPtr(), is_event_driven);
  sampler_->MaybeCollect(
      base::BindPostTaskToCurrentDefault(std::move(on_collected_cb)));
}

void CollectorBase::CheckOnSequence() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}
}  // namespace reporting
