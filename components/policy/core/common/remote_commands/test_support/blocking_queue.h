// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_SUPPORT_BLOCKING_QUEUE_H_
#define COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_SUPPORT_BLOCKING_QUEUE_H_

#include <memory>
#include <queue>
#include <utility>

#include "base/check.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"

namespace policy {
namespace test {

// FIFO Queue that supports pushing and popping of elements from different tasks
// (on the same sequence).
// Only to be used during unittests!
// If no elements are available (yet), the call to Pop() will block until
// an element arrives (through a call to Push()).
//
// All access should happen from the same sequence.
template <typename Type>
class BlockingQueue {
 public:
  BlockingQueue() = default;
  BlockingQueue(const BlockingQueue&) = delete;
  BlockingQueue& operator=(const BlockingQueue&) = delete;
  ~BlockingQueue() = default;

  void Push(Type element) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    elements_.push(std::move(element));
    SignalElementIsAvailable();
  }

  // Waits until an element is available.
  // Returns immediately if elements are already available.
  //
  // Returns true if an element arrived, or false if a timeout happens.
  // A timeout can only happen if |base::test::ScopedRunLoopTimeout| is used in
  // the calling context. In case of a timeout, the test will be failed
  // automatically by |base::test::ScopedRunLoopTimeout|, however if you want to
  // provide a better error message you can always add an explicit check:
  //
  //   ASSERT_TRUE(queue.Wait()) << "Detailed error message";
  //
  bool Wait() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (elements_.empty())
      WaitForElement();

    return !IsEmpty();
  }

  // Returns the first element from this queue (FIFO).
  // If no elements are available, this will wait until one arrives.
  //
  // Will DCHECK if a timeout happens.
  Type Pop() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Ensure an element is available.
    Wait();

    DCHECK(!IsEmpty());
    Type result = elements_.front();
    elements_.pop();
    return result;
  }

  bool IsEmpty() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return elements_.empty();
  }

 private:
  // Wait until a new element is available.
  void WaitForElement() VALID_CONTEXT_REQUIRED(sequence_checker_) {
    DCHECK_EQ(run_loop_, nullptr);

    run_loop_ = std::make_unique<base::RunLoop>();
    // This blocks until 'run_loop_->Quit()' is called.
    run_loop_->Run();
    run_loop_ = nullptr;
  }

  void SignalElementIsAvailable() VALID_CONTEXT_REQUIRED(sequence_checker_) {
    if (run_loop_)
      run_loop_->Quit();
  }

  std::queue<Type> elements_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Used by Wait() to know when Push() is called.
  std::unique_ptr<base::RunLoop> run_loop_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace test
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_REMOTE_COMMANDS_TEST_SUPPORT_BLOCKING_QUEUE_H_
