// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/scheduled_executor.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace nearby::chrome {

class ScheduledExecutor::Core
    : public base::RefCountedThreadSafe<ScheduledExecutor::Core> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  explicit Core(scoped_refptr<base::SequencedTaskRunner> timer_task_runner)
      : timer_task_runner_(std::move(timer_task_runner)) {}

  std::shared_ptr<api::Cancelable> Schedule(Runnable&& runnable,
                                            absl::Duration duration);
  void Shutdown();
  void ShutdownAndRunPendingTasks();
  bool CancelTask(const base::UnguessableToken& id);

 private:
  using TaskEntryMap = absl::flat_hash_map<base::UnguessableToken, Runnable>;
  friend class base::RefCountedThreadSafe<ScheduledExecutor::Core>;
  ~Core();

  void RunTask(const base::UnguessableToken& id);

  scoped_refptr<base::SequencedTaskRunner> timer_task_runner_;

  base::Lock lock_;
  bool is_shut_down_ GUARDED_BY(lock_) = false;
  TaskEntryMap id_to_task_map_ GUARDED_BY(lock_);
};

namespace {

class CancelableTask : public api::Cancelable {
 public:
  CancelableTask(scoped_refptr<ScheduledExecutor::Core> core,
                 const base::UnguessableToken& id)
      : core_(std::move(core)), id_(id) {}
  CancelableTask() = default;
  ~CancelableTask() override = default;

  // api::Cancelable:
  bool Cancel() override {
    if (!core_) {
      return false;
    }
    return core_->CancelTask(id_);
  }

 private:
  scoped_refptr<ScheduledExecutor::Core> core_;
  base::UnguessableToken id_;
};

}  // namespace

ScheduledExecutor::Core::~Core() = default;

std::shared_ptr<api::Cancelable> ScheduledExecutor::Core::Schedule(
    Runnable&& runnable,
    absl::Duration duration) {
  base::UnguessableToken id;
  {
    base::AutoLock al(lock_);
    if (is_shut_down_) {
      return std::make_shared<CancelableTask>();
    }

    // It is highly unlikely that tokens may collide, but it's very easy to
    // handle it.
    do {
      id = base::UnguessableToken::Create();
    } while (!id_to_task_map_.try_emplace(id, std::move(runnable)).second);
  }

  base::TimeDelta delay =
      duration > absl::ZeroDuration()
          ? base::Microseconds(absl::ToInt64Microseconds(duration))
          : base::TimeDelta();

  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ScheduledExecutor::Core::RunTask,
                     base::WrapRefCounted(this), id),
      delay);

  return std::make_shared<CancelableTask>(base::WrapRefCounted(this), id);
}

void ScheduledExecutor::Core::Shutdown() {
  base::AutoLock al(lock_);
  is_shut_down_ = true;
}

void ScheduledExecutor::Core::ShutdownAndRunPendingTasks() {
  TaskEntryMap pending_tasks;
  {
    base::AutoLock al(lock_);
    is_shut_down_ = true;
    using std::swap;
    swap(pending_tasks, id_to_task_map_);
  }

  // Run all tasks prematurely, order does not matter.
  {
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    for (auto& it : pending_tasks) {
      it.second();
    }
  }
}

bool ScheduledExecutor::Core::CancelTask(const base::UnguessableToken& id) {
  Runnable runnable;
  // Move the runnable out of the container under the protection of the lock.
  {
    base::AutoLock al(lock_);
    auto node = id_to_task_map_.extract(id);
    if (node.empty()) {
      return false;
    }
    runnable = std::move(node.mapped());
  }
  // Destroy the runnable outside of the lock. Although the scheduled `Runnable`
  // is destroyed, the delayed task to run it that was posted in
  // `ScheduledExecutor::Core::Schedule()` remains pending in its task queue.
  // This extends the lifetime of this `Core` instance until it is run (it will
  // be a no-op).
  return true;
}

void ScheduledExecutor::Core::RunTask(const base::UnguessableToken& id) {
  Runnable runnable;
  {
    base::AutoLock al(lock_);
    if (auto node = id_to_task_map_.extract(id); !node.empty()) {
      runnable = std::move(node.mapped());
    }
  }

  if (runnable) {
    // base::ScopedAllowBaseSyncPrimitives is required as code inside the
    // runnable uses blocking primitive, which lives outside Chrome.
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    runnable();
  }
}

ScheduledExecutor::ScheduledExecutor(
    scoped_refptr<base::SequencedTaskRunner> timer_task_runner)
    : core_(base::MakeRefCounted<Core>(std::move(timer_task_runner))) {}

ScheduledExecutor::~ScheduledExecutor() {
  core_->ShutdownAndRunPendingTasks();
}

void ScheduledExecutor::Execute(Runnable&& runnable) {
  Schedule(std::move(runnable), absl::ZeroDuration());
}

void ScheduledExecutor::Shutdown() {
  core_->Shutdown();
}

std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(
    Runnable&& runnable,
    absl::Duration duration) {
  return core_->Schedule(std::move(runnable), duration);
}

}  // namespace nearby::chrome
