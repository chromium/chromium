// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACKS_CHILD_CALL_STACK_PROFILE_COLLECTOR_H_
#define COMPONENTS_METRICS_CALL_STACKS_CHILD_CALL_STACK_PROFILE_COLLECTOR_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/metrics/public/mojom/call_stack_profile_collector.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace service_manager {
class InterfaceProvider;
}

namespace metrics {

class SampledProfile;

// ChildCallStackProfileCollector collects stacks at startup, caching them
// internally until a CallStackProfileCollector interface is available. If a
// CallStackProfileCollector is provided via the InterfaceProvider supplied to
// SetParentProfileCollector, the cached stacks are sent via that interface. All
// future stacks received via callbacks supplied by GetProfilerCallback are sent
// via that interface as well.
//
// If no CallStackProfileCollector is provided via InterfaceProvider, any cached
// stacks and all future stacks received via callbacks supplied by
// GetProfilerCallback are flushed. In typical usage this should not happen
// because the browser is expected to always supply a CallStackProfileCollector.
//
// This class is only necessary if a CallStackProfileCollector is not available
// at the time the profiler is created. Otherwise the CallStackProfileCollector
// can be used directly.
//
// CallStackProfileBuilder owns and manages a ChildCallStackProfileCollector. It
// invokes Collect() in CallStackProfileBuilder::OnProfileCompleted() to collect
// a profile.
//
// When the mojo InterfaceProvider becomes available, provide it via
// SetParentProfileCollector().
class ChildCallStackProfileCollector {
 public:
  ChildCallStackProfileCollector();

  ChildCallStackProfileCollector(const ChildCallStackProfileCollector&) =
      delete;
  ChildCallStackProfileCollector& operator=(
      const ChildCallStackProfileCollector&) = delete;

  ~ChildCallStackProfileCollector();

  // Sets the CallStackProfileCollector interface from |parent_collector|. This
  // function MUST be invoked exactly once, regardless of whether
  // |parent_collector| is mojo::NullRemote(), as it flushes pending data in
  // either case.
  void SetParentProfileCollector(
      mojo::PendingRemote<metrics::mojom::CallStackProfileCollector>
          parent_collector);

  // Collects |profile| whose collection start time is |start_timestamp|.
  void Collect(base::TimeTicks start_timestamp, SampledProfile profile);

 private:
  friend class ChildCallStackProfileCollectorTest;

  // Bundles together a collected serialized profile and the collection state
  // for storage, pending availability of the parent mojo interface.
  struct ProfileState {
    ProfileState();
    ProfileState(base::TimeTicks start_timestamp,
                 mojom::ProfileType profile_type,
                 mojom::SampledProfilePtr profile);

    ProfileState(const ProfileState&) = delete;
    ProfileState& operator=(const ProfileState&) = delete;

    ProfileState(ProfileState&&);

    ~ProfileState();

    ProfileState& operator=(ProfileState&&);

    base::TimeTicks start_timestamp;
    mojom::ProfileType profile_type;

    // The serialized sampled profile.
    mojom::SampledProfilePtr profile;
  };

  // This object may be accessed on any thread, including the profiler
  // thread. The expected use case for the object is to be created and have
  // GetProfilerCallback before the message loop starts, which prevents the use
  // of PostTask and the like for inter-thread communication.
  base::Lock lock_;

  // Whether to retain the profile when the interface is not set. Remains true
  // until the invocation of SetParentProfileCollector(), at which point it is
  // false for the rest of the object lifetime.
  bool retain_profiles_ = true;

  // The task runner associated with the parent interface.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The interface to use to collect the stack profiles provided to this
  // object. Initially mojo::NullRemote() until SetParentProfileCollector() is
  // invoked, at which point it may either become set or remain
  // mojo::NullRemote(). If set, stacks are collected via the interface,
  // otherwise they are ignored.
  mojo::Remote<mojom::CallStackProfileCollector> parent_collector_;

  // Profiles being cached by this object, pending a parent interface to be
  // supplied.
  std::vector<ProfileState> profiles_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACKS_CHILD_CALL_STACK_PROFILE_COLLECTOR_H_
