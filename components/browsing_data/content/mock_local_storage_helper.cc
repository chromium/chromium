// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_local_storage_helper.h"

#include <utility>

#include "base/callback.h"
#include "base/containers/contains.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browsing_data {

MockLocalStorageHelper::MockLocalStorageHelper(content::BrowserContext* context)
    : browsing_data::LocalStorageHelper(context) {}

MockLocalStorageHelper::~MockLocalStorageHelper() = default;

void MockLocalStorageHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockLocalStorageHelper::DeleteOrigin(const url::Origin& origin,
                                          base::OnceClosure callback) {
  ASSERT_TRUE(base::Contains(origins_, origin));
  last_deleted_origin_ = origin;
  origins_[origin] = false;
  std::move(callback).Run();
}

void MockLocalStorageHelper::AddLocalStorageSamples() {
  const GURL kOrigin1("http://host1:1/");
  const GURL kOrigin2("http://host2:2/");
  AddLocalStorageForOrigin(url::Origin::Create(kOrigin1), 1);
  AddLocalStorageForOrigin(url::Origin::Create(kOrigin2), 2);
}

void MockLocalStorageHelper::AddLocalStorageForOrigin(const url::Origin& origin,
                                                      int64_t size) {
  response_.emplace_back(origin, size, base::Time());
  origins_[origin] = true;
}

void MockLocalStorageHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockLocalStorageHelper::Reset() {
  for (auto& pair : origins_)
    pair.second = true;
}

bool MockLocalStorageHelper::AllDeleted() {
  for (const auto& pair : origins_) {
    if (pair.second)
      return false;
  }
  return true;
}

}  // namespace browsing_data
