// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_REFCOUNTED_CLOSURE_LIST_H_
#define COMPONENTS_REPORTING_UTIL_REFCOUNTED_CLOSURE_LIST_H_

#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"

namespace reporting {

// Thread-safe refcounted closure class, combining `BarrierClosure` and
// `OnceClosureList`, but allowing to take and drop references at arbitrary
// time. When the last reference is dropped, `OnceClosureList` is notified.
class RefCountedClosureList
    : public base::RefCountedDeleteOnSequence<RefCountedClosureList> {
 public:
  explicit RefCountedClosureList(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);
  RefCountedClosureList(const RefCountedClosureList&) = delete;
  RefCountedClosureList& operator=(const RefCountedClosureList&) = delete;

  // Registers completion notification callback.
  void RegisterCompletionCallback(base::OnceClosure callback);

 private:
  friend class base::RefCountedDeleteOnSequence<RefCountedClosureList>;
  friend class base::DeleteHelper<RefCountedClosureList>;

  ~RefCountedClosureList();

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // List of all completion closures (protected by |sequenced_task_runner_|).
  std::vector<base::OnceClosure> callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_REFCOUNTED_CLOSURE_LIST_H_
