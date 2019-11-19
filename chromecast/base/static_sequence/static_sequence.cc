// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/static_sequence/static_sequence.h"

namespace util {
namespace internal {

StaticTaskRunnerHolder::StaticTaskRunnerHolder(base::TaskTraits traits)
    : traits_(traits), initialized_(false) {}

StaticTaskRunnerHolder::~StaticTaskRunnerHolder() = default;

void StaticTaskRunnerHolder::WillDestroyCurrentMessageLoop() {
  initialized_ = false;
  task_runner_ = nullptr;
}

const scoped_refptr<base::SequencedTaskRunner>& StaticTaskRunnerHolder::Get() {
  if (!initialized_) {
    task_runner_ = base::CreateSequencedTaskRunner(traits_);
    base::MessageLoopCurrent::Get().AddDestructionObserver(this);
    initialized_ = true;
  }
  return task_runner_;
}

}  // namespace internal
}  // namespace util
