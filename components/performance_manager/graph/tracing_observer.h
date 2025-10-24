// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_TRACING_OBSERVER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_TRACING_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/trace_event/trace_event.h"
#include "components/performance_manager/public/graph/graph_registered.h"

namespace performance_manager {

class TracingObserver : public base::CheckedObserver {
 public:
  // Called when a track event tracing session is started. It is possible to
  // emit track events from this callback.
  virtual void OnTraceSessionStart() {}
};

class TracingObserverList : public GraphOwnedAndRegistered<TracingObserverList>,
                            public perfetto::TrackEventSessionObserver {
 public:
  TracingObserverList();
  ~TracingObserverList() override;

  // Register |observer| to get tracing notifications.
  void AddObserver(TracingObserver* observer);
  // Unregister previously registered |observer|.
  void RemoveObserver(TracingObserver* observer);

  void OnStart(const perfetto::DataSourceBase::StartArgs&) override;

 protected:
  void NotifyObservers();

  base::ObserverList<TracingObserver> observers_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::RepeatingClosure notify_closure_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<TracingObserverList> weak_ptr_factory_{this};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_TRACING_OBSERVER_H_
