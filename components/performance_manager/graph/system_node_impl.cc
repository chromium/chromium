// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/system_node_impl.h"

#include <algorithm>
#include <iterator>

#include "base/containers/flat_set.h"
#include "base/process/process_handle.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/graph_impl_operations.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"

namespace performance_manager {

SystemNodeImpl::SystemNodeImpl() {
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&SystemNodeImpl::OnMemoryPressure,
                                     base::Unretained(this)));
}

SystemNodeImpl::~SystemNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SystemNodeImpl::RemoveNodeAttachedData() {}

void SystemNodeImpl::OnProcessMemoryMetricsAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : GetObservers()) {
    observer.OnProcessMemoryMetricsAvailable(this);
  }
}

void SystemNodeImpl::OnMemoryPressure(MemoryPressureLevel new_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& observer : GetObservers()) {
    observer.OnBeforeMemoryPressure(new_level);
  }
  for (auto& observer : GetObservers()) {
    observer.OnMemoryPressure(new_level);
  }
}

}  // namespace performance_manager
