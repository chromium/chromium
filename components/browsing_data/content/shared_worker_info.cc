// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/browsing_data/content/shared_worker_info.h"

namespace browsing_data {

SharedWorkerInfo::SharedWorkerInfo(
    const GURL& worker,
    const std::string& name,
    const blink::StorageKey& storage_key,
    const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies)
    : worker(worker),
      name(name),
      storage_key(storage_key),
      same_site_cookies(same_site_cookies) {}

SharedWorkerInfo::SharedWorkerInfo(const SharedWorkerInfo& other) = default;

SharedWorkerInfo::~SharedWorkerInfo() = default;

bool SharedWorkerInfo::operator==(const SharedWorkerInfo& other) const {
  return std::tie(worker, name, storage_key, same_site_cookies) ==
         std::tie(other.worker, other.name, other.storage_key,
                  other.same_site_cookies);
}

bool SharedWorkerInfo::operator<(const SharedWorkerInfo& other) const {
  return std::tie(worker, name, storage_key, same_site_cookies) <
         std::tie(other.worker, other.name, other.storage_key,
                  other.same_site_cookies);
}

}  // namespace browsing_data
