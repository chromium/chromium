// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_SHARED_WORKER_INFO_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_SHARED_WORKER_INFO_H_

#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace browsing_data {
// Contains information about a Shared Worker.
struct SharedWorkerInfo {
  SharedWorkerInfo(const GURL& worker,
                   const std::string& name,
                   const blink::StorageKey& storage_key);
  SharedWorkerInfo(const SharedWorkerInfo& other);
  ~SharedWorkerInfo();

  bool operator==(const SharedWorkerInfo& other) const;
  bool operator<(const SharedWorkerInfo& other) const;

  GURL worker;
  std::string name;
  blink::StorageKey storage_key;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_SHARED_WORKER_INFO_H_
