// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/noop_store.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/download/internal/background_service/entry.h"

namespace download {

NoopStore::NoopStore() : initialized_(false) {}

NoopStore::~NoopStore() = default;

bool NoopStore::IsInitialized() {
  return initialized_;
}

void NoopStore::Initialize(InitCallback callback) {
  DCHECK(!IsInitialized());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&NoopStore::OnInitFinished, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void NoopStore::HardRecover(StoreCallback callback) {
  initialized_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void NoopStore::Update(const Entry& entry, StoreCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /** success */));
}

void NoopStore::Remove(const std::string& guid, StoreCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /** success */));
}

void NoopStore::OnInitFinished(InitCallback callback) {
  initialized_ = true;

  std::unique_ptr<std::vector<Entry>> entries =
      std::make_unique<std::vector<Entry>>();
  std::move(callback).Run(true /** success */, std::move(entries));
}

}  // namespace download
