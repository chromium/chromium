// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/refcounted_closure_list.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"

namespace reporting {

RefCountedClosureList::RefCountedClosureList(
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : base::RefCountedDeleteOnSequence<RefCountedClosureList>(
          sequenced_task_runner),
      sequenced_task_runner_(sequenced_task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RefCountedClosureList::~RefCountedClosureList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Send notification to all registered closures.
  for (auto& callback : callbacks_) {
    std::move(callback).Run();
  }
  callbacks_.clear();
}

void RefCountedClosureList::RegisterCompletionCallback(
    base::OnceClosure callback) {
  CHECK(callback);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callbacks_.push_back(std::move(callback));
}

}  // namespace reporting
