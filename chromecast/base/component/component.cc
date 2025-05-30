// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/component/component.h"

#include <atomic>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"

namespace chromecast {

namespace {

const int32_t kEnabledBit = 0x40000000;

}  // namespace

namespace subtle {

class DependencyCount : public base::RefCountedThreadSafe<DependencyCount> {
 public:
  explicit DependencyCount(ComponentBase* component)
      : component_(component),
        task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        dep_count_(0),
        disabling_(false) {
    DCHECK(component_);
  }

  DependencyCount(const DependencyCount&) = delete;
  DependencyCount& operator=(const DependencyCount&) = delete;

  void Detach() {
    DCHECK(task_runner_->BelongsToCurrentThread());
    component_ = nullptr;
  }

  void Disable() {
    DCHECK(task_runner_->BelongsToCurrentThread());
    DCHECK(!disabling_);
    disabling_ = true;

    std::set<DependencyBase*> dependents(strong_dependents_);
    for (DependencyBase* dependent : dependents)
      dependent->Disable();

    intptr_t old_deps = dep_count_.fetch_and(~kEnabledBit);
    if ((old_deps & ~kEnabledBit) == 0)
      DisableComplete();
  }

  void Enable() {
    DCHECK(task_runner_->BelongsToCurrentThread());
    disabling_ = false;
    intptr_t old_deps = dep_count_.fetch_or(kEnabledBit);
    DCHECK(!(old_deps & kEnabledBit));

    for (DependencyBase* dependent : strong_dependents_)
      dependent->Ready(component_);
  }

  ComponentBase* WeakAcquireDep() {
    while (true) {
      intptr_t deps = dep_count_.load(std::memory_order_relaxed);
      if (!(deps & kEnabledBit))
        return nullptr;

      bool success = dep_count_.compare_exchange_strong(deps, deps + 1,
          std::memory_order_acquire);
      // We depend on the fact that a component must be disabled (meaning that
      // we will never reach this point) before it is destroyed. Therefore if
      // we do reach this point, it is safe to return the raw pointer.
      if (success)
        return component_;
    }
  }

  void StrongAcquireDep(DependencyBase* dependent) {
    DCHECK(dependent);
    DCHECK(task_runner_->BelongsToCurrentThread());
    if (!component_) {
      dependent->Disable();
      return;
    }

    strong_dependents_.insert(dependent);
    intptr_t count = dep_count_.fetch_add(1, std::memory_order_relaxed) + 1;
    DCHECK_GT(count, 0);

    if (count & kEnabledBit) {
      dependent->Ready(component_);
    } else {
      component_->Enable();
    }
  }

  void StrongReleaseDep(DependencyBase* dependent) {
    DCHECK(dependent);
    DCHECK(task_runner_->BelongsToCurrentThread());
    strong_dependents_.erase(dependent);
    ReleaseDep();
  }

  void ReleaseDep() {
    intptr_t after = dep_count_.fetch_sub(1) - 1;
    DCHECK_GE(after, 0);
    if (after == 0)
      DisableComplete();
  }

  bool DependsOn(ComponentBase* component) {
    DCHECK(task_runner_->BelongsToCurrentThread());
    if (!component_)
      return false;
    if (component_ == component)
      return true;
    return component_->DependsOn(component);
  }

 private:
  friend class base::RefCountedThreadSafe<DependencyCount>;

  ~DependencyCount() {}

  void DisableComplete() {
    if (!task_runner_->BelongsToCurrentThread()) {
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&DependencyCount::DisableComplete, this));
      return;
    }
    // Need to make sure that Enable() was not called in the meantime.
    if (dep_count_.load(std::memory_order_relaxed) != 0 || !disabling_)
      return;
    // Ensure that we don't call DisableComplete() more than once per Disable().
    disabling_ = false;
    DCHECK(component_);
    DCHECK(strong_dependents_.empty());
    component_->DependencyCountDisableComplete();
  }

  raw_ptr<ComponentBase> component_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::atomic<intptr_t> dep_count_;
  bool disabling_;
  std::set<DependencyBase*> strong_dependents_;
};

DependencyBase::DependencyBase(const WeakReferenceBase& dependency,
                               ComponentBase* dependent)
    : dependent_(dependent),
      dependency_(nullptr),
      counter_(dependency.counter_) {
  DCHECK(dependent_);
  dependent_->AddDependency(this);
}

DependencyBase::~DependencyBase() {}

void DependencyBase::StartUsing() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!dependency_);
  counter_->StrongAcquireDep(this);
}

void DependencyBase::StopUsing() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!dependency_)
    return;
  dependency_ = nullptr;
  counter_->StrongReleaseDep(this);
}

void DependencyBase::Ready(ComponentBase* dependency) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!dependency_);
  DCHECK(dependency);
  dependency_ = dependency;
  dependent_->DependencyReady();
}

void DependencyBase::Disable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  dependent_->Disable();
}

bool DependencyBase::DependsOn(ComponentBase* component) {
  return counter_->DependsOn(component);
}

WeakReferenceBase::WeakReferenceBase(const ComponentBase& dependency)
    : counter_(dependency.counter_) {
  DCHECK(counter_);
}

WeakReferenceBase::WeakReferenceBase(const DependencyBase& dependency)
    : counter_(dependency.counter_) {
  DCHECK(counter_);
}

WeakReferenceBase::WeakReferenceBase(const WeakReferenceBase& other)
    : counter_(other.counter_) {
  DCHECK(counter_);
}

WeakReferenceBase::WeakReferenceBase(WeakReferenceBase&& other)
    : counter_(std::move(other.counter_)) {
  DCHECK(counter_);
}

WeakReferenceBase::~WeakReferenceBase() {}

ScopedReferenceBase::ScopedReferenceBase(
    const scoped_refptr<DependencyCount>& counter)
    : counter_(counter) {
  DCHECK(counter_);
  dependency_ = counter_->WeakAcquireDep();
}

ScopedReferenceBase::ScopedReferenceBase(ScopedReferenceBase&& other)
    : counter_(std::move(other.counter_)), dependency_(other.dependency_) {
  DCHECK(counter_);
  other.dependency_ = nullptr;
}

ScopedReferenceBase::~ScopedReferenceBase() {
  if (dependency_)
    counter_->ReleaseDep();
}

}  // namespace subtle

ComponentBase::ComponentBase()
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      state_(kStateDisabled),
      async_call_in_progress_(false),
      pending_dependency_count_(0),
      observers_(new base::ObserverListThreadSafe<Observer>()) {
  counter_ = new subtle::DependencyCount(this);
}

ComponentBase::~ComponentBase() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(kStateDisabled, state_) << "Components must be disabled "
                                    << "before being destroyed";
  counter_->Detach();
}

void ComponentBase::Enable() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ == kStateEnabling || state_ == kStateEnabled ||
      state_ == kStateDestroying) {
    return;
  }
  state_ = kStateEnabling;

  if (strong_dependencies_.empty()) {
    TryOnEnable();
  } else {
    // Enable all strong dependencies first.
    pending_dependency_count_ = strong_dependencies_.size();
    for (subtle::DependencyBase* dependency : strong_dependencies_)
      dependency->StartUsing();
  }
}

void ComponentBase::DependencyReady() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ != kStateEnabling)
    return;
  DCHECK_GT(pending_dependency_count_, 0);
  --pending_dependency_count_;
  if (pending_dependency_count_ == 0)
    TryOnEnable();
}

void ComponentBase::TryOnEnable() {
  DCHECK_EQ(kStateEnabling, state_);
  if (async_call_in_progress_)
    return;
  async_call_in_progress_ = true;
  OnEnable();
}

void ComponentBase::OnEnableComplete(bool success) {
  // Always post a task, to prevent the stack from getting too deep.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ComponentBase::OnEnableCompleteInternal,
                                base::Unretained(this), success));
}

void ComponentBase::OnEnableCompleteInternal(bool success) {
  async_call_in_progress_ = false;
  DCHECK(state_ == kStateEnabling || state_ == kStateDisabling ||
         state_ == kStateDestroying);
  if (state_ != kStateEnabling) {
    if (success) {
      TryOnDisable();
    } else {
      OnDisableCompleteInternal();
    }
    return;
  }

  if (success) {
    state_ = kStateEnabled;
    counter_->Enable();
  } else {
    Disable();
  }
  observers_->Notify(FROM_HERE, &Observer::OnComponentEnabled, this, success);
}

void ComponentBase::Destroy() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(kStateDestroying, state_);
  if (state_ == kStateDisabled) {
    delete this;
  } else {
    bool should_disable = (state_ != kStateDisabling);
    state_ = kStateDestroying;
    if (should_disable)
      counter_->Disable();
  }
}

void ComponentBase::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_->AddObserver(observer);
}

void ComponentBase::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
}

void ComponentBase::Disable() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ == kStateDisabling || state_ == kStateDisabled ||
      state_ == kStateDestroying) {
    return;
  }
  state_ = kStateDisabling;
  counter_->Disable();
}

void ComponentBase::DependencyCountDisableComplete() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (state_ == kStateDisabling || state_ == kStateDestroying)
    TryOnDisable();
}

void ComponentBase::TryOnDisable() {
  DCHECK(state_ == kStateDisabling || state_ == kStateDestroying);
  if (async_call_in_progress_)
    return;
  async_call_in_progress_ = true;
  OnDisable();
}

void ComponentBase::OnDisableComplete() {
  // Always post a task, to prevent calls to Disable() from within Enable().
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ComponentBase::OnDisableCompleteInternal,
                                base::Unretained(this)));
}

void ComponentBase::OnDisableCompleteInternal() {
  async_call_in_progress_ = false;
  DCHECK(state_ == kStateEnabling || state_ == kStateDisabling ||
         state_ == kStateDestroying);
  if (state_ == kStateEnabling) {
    TryOnEnable();
    return;
  }

  if (state_ == kStateDestroying) {
    StopUsingDependencies();
    observers_->Notify(FROM_HERE, &Observer::OnComponentDisabled, this);
    state_ = kStateDisabled;
    delete this;
  } else {
    state_ = kStateDisabled;
    StopUsingDependencies();
    observers_->Notify(FROM_HERE, &Observer::OnComponentDisabled, this);
  }
}

void ComponentBase::AddDependency(subtle::DependencyBase* dependency) {
  DCHECK_EQ(kStateDisabled, state_);
  DCHECK(!dependency->DependsOn(this)) << "Circular dependency detected";
  strong_dependencies_.push_back(dependency);
}

void ComponentBase::StopUsingDependencies() {
  for (subtle::DependencyBase* dependency : strong_dependencies_)
    dependency->StopUsing();
}

bool ComponentBase::DependsOn(ComponentBase* component) {
  for (subtle::DependencyBase* dependency : strong_dependencies_) {
    if (dependency->DependsOn(component))
      return true;
  }
  return false;
}

}  // namespace chromecast
