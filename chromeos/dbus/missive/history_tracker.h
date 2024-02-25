// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MISSIVE_HISTORY_TRACKER_H_
#define CHROMEOS_DBUS_MISSIVE_HISTORY_TRACKER_H_

#include <atomic>

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/proto/synced/health.pb.h"

namespace reporting {

// Asynchronous container for health tracking data.
// Attached to ReportClient, accessed/modified on certain dBus calls.
class HistoryTracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnNewData(const ERPHealthData& data) const = 0;
  };

  HistoryTracker(const HistoryTracker&) = delete;
  HistoryTracker& operator=(const HistoryTracker&) = delete;
  ~HistoryTracker();

  // Singleton.
  static HistoryTracker* Get();

  // Observer handling. Must happen on `sequenced_task_runner_`.
  // All regustered observers are called on `sequenced_task_runner_` too.
  void AddObserver(Observer* observer);
  void RemoveObserver(const Observer* observer);

  // Accessors.
  bool debug_state() const;
  void set_debug_state(bool state);

  void retrieve_data(base::OnceCallback<void(const ERPHealthData&)> cb);
  void set_data(ERPHealthData data, base::OnceClosure cb);

 private:
  // To be called by singleton accessor only.
  explicit HistoryTracker(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  std::atomic<bool> debug_state_{false};

  ERPHealthData data_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::ObserverList<Observer> observer_list_
      GUARDED_BY_CONTEXT(sequence_checker_);
};
}  // namespace reporting

#endif  // CHROMEOS_DBUS_MISSIVE_HISTORY_TRACKER_H_
