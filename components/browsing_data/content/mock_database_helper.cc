// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_database_helper.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

MockDatabaseHelper::MockDatabaseHelper(content::BrowserContext* browser_context)
    : DatabaseHelper(browser_context) {}

MockDatabaseHelper::~MockDatabaseHelper() {}

void MockDatabaseHelper::StartFetching(FetchCallback callback) {
  callback_ = std::move(callback);
}

void MockDatabaseHelper::DeleteDatabase(const url::Origin& origin) {
  const std::string identifier = storage::GetIdentifierFromOrigin(origin);
  ASSERT_TRUE(base::Contains(databases_, identifier));
  last_deleted_origin_ = identifier;
  databases_[identifier] = false;
}

void MockDatabaseHelper::AddDatabaseSamples() {
  response_.emplace_back(
      blink::StorageKey(url::Origin::Create(GURL("http://gdbhost1:1"))), 1,
      base::Time());
  databases_["http_gdbhost1_1"] = true;
  response_.emplace_back(
      blink::StorageKey(url::Origin::Create(GURL("http://gdbhost2:2"))), 2,
      base::Time());
  databases_["http_gdbhost2_2"] = true;
}

void MockDatabaseHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockDatabaseHelper::Reset() {
  for (auto& pair : databases_)
    pair.second = true;
}

bool MockDatabaseHelper::AllDeleted() {
  for (const auto& pair : databases_) {
    if (pair.second)
      return false;
  }
  return true;
}

}  // namespace browsing_data
