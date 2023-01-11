// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/in_memory_event_store.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/feature_engagement/internal/event_store.h"

namespace feature_engagement {

InMemoryEventStore::InMemoryEventStore(
    std::unique_ptr<std::vector<Event>> events)
    : events_(std::move(events)), ready_(false) {}

InMemoryEventStore::InMemoryEventStore()
    : InMemoryEventStore(std::make_unique<std::vector<Event>>()) {}

InMemoryEventStore::~InMemoryEventStore() = default;

void InMemoryEventStore::Load(OnLoadedCallback callback) {
  HandleLoadResult(std::move(callback), true);
}

bool InMemoryEventStore::IsReady() const {
  return ready_;
}

void InMemoryEventStore::WriteEvent(const Event& event) {
  // Intentionally ignore all writes.
}

void InMemoryEventStore::DeleteEvent(const std::string& event_name) {
  // Intentionally ignore all deletes.
}

void InMemoryEventStore::HandleLoadResult(OnLoadedCallback callback,
                                          bool success) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, std::move(events_)));
  ready_ = success;
}

}  // namespace feature_engagement
