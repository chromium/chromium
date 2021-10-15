// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_SHARED_QUEUE_H_
#define COMPONENTS_REPORTING_UTIL_SHARED_QUEUE_H_

#include <utility>

#include "base/containers/queue.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// SharedQueue wraps a |base::queue| and ensures access happens on a
// SequencedTaskRunner.
template <typename QueueType>
class SharedQueue : public base::RefCountedThreadSafe<SharedQueue<QueueType>> {
 public:
  static scoped_refptr<SharedQueue<QueueType>> Create(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
          base::ThreadPool::CreateSequencedTaskRunner({})) {
    return base::WrapRefCounted(
        new SharedQueue<QueueType>(sequenced_task_runner));
  }

  // Push will schedule a push of |item| onto the queue and call
  // |push_complete_cb| once complete.
  void Push(QueueType item, base::OnceCallback<void()> push_complete_cb) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SharedQueue::OnPush, this, std::move(item),
                                  std::move(push_complete_cb)));
  }

  // Pop will schedule a pop off the queue and call |get_pop_cb| once complete.
  // If the queue is empty, |get_pop_cb| will be called with
  // error::OUT_OF_RANGE.
  void Pop(base::OnceCallback<void(StatusOr<QueueType>)> get_pop_cb) {
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SharedQueue::OnPop, this, std::move(get_pop_cb)));
  }

  // Swap will schedule a swap of the |queue_| contents with the provided
  // |new_queue|, and send the old contents to the |swap_queue_cb|.
  void Swap(base::queue<QueueType> new_queue,
            base::OnceCallback<void(base::queue<QueueType>)> swap_queue_cb) {
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SharedQueue::OnSwap, this, std::move(new_queue),
                       std::move(swap_queue_cb)));
  }

 protected:
  virtual ~SharedQueue() = default;

 private:
  friend class base::RefCountedThreadSafe<SharedQueue<QueueType>>;

  explicit SharedQueue(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
      : sequenced_task_runner_(sequenced_task_runner) {}

  void OnPush(QueueType item, base::OnceCallback<void()> push_complete_cb) {
    queue_.push(std::move(item));
    std::move(push_complete_cb).Run();
  }

  void OnPop(base::OnceCallback<void(StatusOr<QueueType>)> cb) {
    if (queue_.empty()) {
      std::move(cb).Run(Status(error::OUT_OF_RANGE, "Queue is empty"));
      return;
    }

    QueueType item = std::move(queue_.front());
    queue_.pop();
    std::move(cb).Run(std::move(item));
  }

  void OnSwap(base::queue<QueueType> new_queue,
              base::OnceCallback<void(base::queue<QueueType>)> swap_queue_cb) {
    queue_.swap(new_queue);
    std::move(swap_queue_cb).Run(std::move(new_queue));
  }

  // Used to monitor if the callback is in use or not.
  base::queue<QueueType> queue_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_SHARED_QUEUE_H_
