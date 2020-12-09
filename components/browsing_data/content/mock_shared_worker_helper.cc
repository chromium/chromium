// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_shared_worker_helper.h"

#include "base/callback.h"
#include "base/containers/contains.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

MockSharedWorkerHelper::MockSharedWorkerHelper(
    content::BrowserContext* browser_context)
    : SharedWorkerHelper(
          content::BrowserContext::GetDefaultStoragePartition(browser_context),
          browser_context->GetResourceContext()) {}

MockSharedWorkerHelper::~MockSharedWorkerHelper() {}

void MockSharedWorkerHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockSharedWorkerHelper::DeleteSharedWorker(
    const GURL& worker,
    const std::string& name,
    const url::Origin& constructor_origin) {
  SharedWorkerInfo key(worker, name, constructor_origin);
  ASSERT_TRUE(base::Contains(workers_, key));
  workers_[key] = false;
}

void MockSharedWorkerHelper::AddSharedWorkerSamples() {
  GURL worker1("https://sharedworkerhost1:1/app/worker.js");
  std::string name1("my worker");
  url::Origin constructor_origin1 = url::Origin::Create(worker1);
  GURL worker2("https://sharedworkerhost2:2/worker.js");
  std::string name2("another worker");
  url::Origin constructor_origin2 = url::Origin::Create(worker2);

  response_.push_back({worker1, name1, constructor_origin1});
  response_.push_back({worker2, name2, constructor_origin2});
  workers_[{worker1, name1, constructor_origin1}] = true;
  workers_[{worker2, name2, constructor_origin2}] = true;
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
