// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_shared_worker_helper.h"

#include "base/callback.h"
#include "base/containers/contains.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace browsing_data {

MockSharedWorkerHelper::MockSharedWorkerHelper(
    content::BrowserContext* browser_context)
    : SharedWorkerHelper(browser_context->GetDefaultStoragePartition()) {}

MockSharedWorkerHelper::~MockSharedWorkerHelper() {}

void MockSharedWorkerHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockSharedWorkerHelper::DeleteSharedWorker(
    const GURL& worker,
    const std::string& name,
    const blink::StorageKey& storage_key) {
  SharedWorkerInfo key(worker, name, storage_key);
  ASSERT_TRUE(base::Contains(workers_, key));
  workers_[key] = false;
}

void MockSharedWorkerHelper::AddSharedWorkerSamples() {
  GURL worker1("https://sharedworkerhost1:1/app/worker.js");
  std::string name1("my worker");
  blink::StorageKey storage_key1(url::Origin::Create(worker1));
  GURL worker2("https://sharedworkerhost2:2/worker.js");
  std::string name2("another worker");
  blink::StorageKey storage_key2(url::Origin::Create(worker2));

  response_.emplace_back(worker1, name1, storage_key1);
  response_.emplace_back(worker2, name2, storage_key2);
  workers_[{worker1, name1, storage_key1}] = true;
  workers_[{worker2, name2, storage_key2}] = true;
}

void MockSharedWorkerHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockSharedWorkerHelper::Reset() {
  for (auto& pair : workers_)
    pair.second = true;
}

bool MockSharedWorkerHelper::AllDeleted() {
  for (const auto& pair : workers_) {
    if (pair.second)
      return false;
  }
  return true;
}

}  // namespace browsing_data
