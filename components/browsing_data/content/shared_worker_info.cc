// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/browsing_data/content/shared_worker_info.h"

namespace browsing_data {

SharedWorkerInfo::SharedWorkerInfo(const GURL& worker,
                                   const std::string& name,
                                   const blink::StorageKey& storage_key)
    : worker(worker), name(name), storage_key(storage_key) {}

SharedWorkerInfo::SharedWorkerInfo(const SharedWorkerInfo& other) = default;

SharedWorkerInfo::~SharedWorkerInfo() = default;

bool SharedWorkerInfo::operator==(const SharedWorkerInfo& other) const {
  return std::tie(worker, name, storage_key) ==
         std::tie(other.worker, other.name, other.storage_key);
}

bool SharedWorkerInfo::operator<(const SharedWorkerInfo& other) const {
  return std::tie(worker, name, storage_key) <
         std::tie(other.worker, other.name, other.storage_key);
}

}  // namespace browsing_data
