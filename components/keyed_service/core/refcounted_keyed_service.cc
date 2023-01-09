// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "base/task/sequenced_task_runner.h"

#include <utility>

namespace impl {

// static
void RefcountedKeyedServiceTraits::Destruct(const RefcountedKeyedService* obj) {
  if (obj->task_runner_ && !obj->task_runner_->RunsTasksInCurrentSequence()) {
    obj->task_runner_->DeleteSoon(FROM_HERE, obj);
  } else {
    delete obj;
  }
}

}  // namespace impl

RefcountedKeyedService::RefcountedKeyedService() : task_runner_(nullptr) {
}

RefcountedKeyedService::RefcountedKeyedService(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

RefcountedKeyedService::~RefcountedKeyedService() = default;
