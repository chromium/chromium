// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_INITIALIZATION_TASK_H_
#define COMPONENTS_PAYMENTS_CONTENT_INITIALIZATION_TASK_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace payments {

// An interface for a task that takes time to initialize. Useful for monitoring
// initialization of several asynchronous tasks.
//
// Sample usage:
//
//   class Foo : public InitializationTask {
//    public:
//     Foo() {}
//
//     ~Foo() override {}
//
//     // InitializationTask:
//     bool IsInitialized() override {
//       return is_initialized_;
//     }
//
//     void SomeAction() {
//       is_initialized_ = true;
//       NotifyInitialized();
//     }
//
//    private:
//     bool is_initialized_ = false;
//   };
class InitializationTask {
 public:
  // An interface for an observer of an initialization task.
  //
  // Sample usage:
  //
  //   class Bar : public InitializationTask::Observer {
  //    public:
  //     explicit Bar(Foo* foo) : foo_(foo) {
  //       if (foo_->IsInitialized()) {
  //         UseFoo();
  //       } else {
  //         foo_->AddInitializationObserver(this);
  //       }
  //     }
  //
  //     ~Bar() override {}
  //
  //     // InitializationTask::Observer:
  //     void OnInitialized(InitializationTask* initialization_task) override {
  //       initialization_task->RemoveInitializationObserver(this);
  //       UseFoo();
  //     }
  //
  //     void UseFoo() {
  //       foo_->DoSomethingInteresting();
  //     }
  //
  //     private:
  //       // Not owned. Must outlive Bar.
  //       Foo* foo_;
  //   };
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    // Called when the observed task has initialized.
    virtual void OnInitialized(InitializationTask* initialization_task) = 0;
  };

  InitializationTask();

  InitializationTask(const InitializationTask&) = delete;
  InitializationTask& operator=(const InitializationTask&) = delete;

  virtual ~InitializationTask();

  // Add the |observer| to be notified of initialization.
  void AddInitializationObserver(Observer* observer);

  // Remove the |observer| of initialization.
  void RemoveInitializationObserver(Observer* observer);

  // Notify all observers of initialization. Should be called at most once.
  void NotifyInitialized();

  // Whether the task has initialized.
  virtual bool IsInitialized() const = 0;

 private:
  // The list of observers for this initialization task.
  base::ObserverList<Observer> observers_;

  // Whether NotifyInitialized() has been called.
  bool has_notified_ = false;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_INITIALIZATION_TASK_H_
