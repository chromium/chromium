// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/child_call_stack_profile_collector.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

ChildCallStackProfileCollector::ProfileState::ProfileState() = default;
ChildCallStackProfileCollector::ProfileState::ProfileState(ProfileState&&) =
    default;

ChildCallStackProfileCollector::ProfileState::ProfileState(
    base::TimeTicks start_timestamp,
    std::string profile)
    : start_timestamp(start_timestamp), profile(std::move(profile)) {}

ChildCallStackProfileCollector::ProfileState::~ProfileState() = default;

// Some versions of GCC need this for push_back to work with std::move.
ChildCallStackProfileCollector::ProfileState&
ChildCallStackProfileCollector::ProfileState::operator=(ProfileState&&) =
    default;

ChildCallStackProfileCollector::ChildCallStackProfileCollector() {}

ChildCallStackProfileCollector::~ChildCallStackProfileCollector() {}

void ChildCallStackProfileCollector::SetParentProfileCollector(
    mojo::PendingRemote<metrics::mojom::CallStackProfileCollector>
        parent_collector) {
  base::AutoLock alock(lock_);
  // This function should only invoked once, during the mode of operation when
  // retaining profiles after construction.
  DCHECK(retain_profiles_);
  retain_profiles_ = false;
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  // This should only be set one time per child process.
  DCHECK(!parent_collector_);
  // If |parent_collector| is mojo::NullRemote(), it skips Bind since
  // mojo::Remote doesn't allow Bind with mojo::NullRemote().
  if (parent_collector) {
    parent_collector_.Bind(std::move(parent_collector));
    if (parent_collector_) {
      for (ProfileState& state : profiles_) {
        mojom::SampledProfilePtr mojo_profile = mojom::SampledProfile::New();
        mojo_profile->contents = std::move(state.profile);
        parent_collector_->Collect(state.start_timestamp,
                                   std::move(mojo_profile));
      }
    }
  }
  profiles_.clear();
}

void ChildCallStackProfileCollector::Collect(base::TimeTicks start_timestamp,
                                             SampledProfile profile) {
  base::AutoLock alock(lock_);
  if (task_runner_ &&
      // The profiler thread does not have a task runner. Attempting to
      // invoke Get() on it results in a DCHECK.
      (!base::ThreadTaskRunnerHandle::IsSet() ||
       base::ThreadTaskRunnerHandle::Get() != task_runner_)) {
    // Post back to the thread that owns the the parent interface.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChildCallStackProfileCollector::Collect,
                                  // This class has lazy instance lifetime.
                                  base::Unretained(this), start_timestamp,
                                  std::move(profile)));
    return;
  }

  if (parent_collector_) {
    mojom::SampledProfilePtr mojo_profile = mojom::SampledProfile::New();
    profile.SerializeToString(&mojo_profile->contents);
    parent_collector_->Collect(start_timestamp, std::move(mojo_profile));
    return;
  }

  if (retain_profiles_) {
    std::string serialized_profile;
    profile.SerializeToString(&serialized_profile);
    profiles_.emplace_back(start_timestamp, std::move(serialized_profile));
  }
}

}  // namespace metrics
