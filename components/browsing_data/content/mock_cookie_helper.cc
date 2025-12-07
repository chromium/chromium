// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_cookie_helper.h"

#include <memory>
#include <optional>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "net/cookies/cookie_options.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

MockCookieHelper::MockCookieHelper(content::StoragePartition* storage_partition)
    : CookieHelper(storage_partition, base::NullCallback()) {}

MockCookieHelper::~MockCookieHelper() = default;

void MockCookieHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockCookieHelper::DeleteCookie(const net::CanonicalCookie& cookie) {
  ASSERT_TRUE(base::Contains(cookies_, cookie));
  cookies_[cookie] = false;
}

void MockCookieHelper::AddCookieSamples(
    const GURL& url,
    const std::string& cookie_line,
    std::optional<net::CookiePartitionKey> cookie_partition_key) {
  std::unique_ptr<net::CanonicalCookie> cc(
      net::CanonicalCookie::CreateForTesting(
          url, cookie_line, base::Time::Now(), std::nullopt /* server_time */,
          cookie_partition_key));

  if (cc.get()) {
    if (cookies_.count(*cc))
      return;
    cookie_list_.push_back(*cc);
    cookies_[*cc] = true;
  }
}

void MockCookieHelper::Notify() {
  if (!callback_.is_null())
    std::move(callback_).Run(cookie_list_);
}

void MockCookieHelper::Reset() {
  for (auto& pair : cookies_)
    pair.second = true;
}

bool MockCookieHelper::AllDeleted() {
  for (const auto& pair : cookies_) {
    if (pair.second)
      return false;
  }
  return true;
}

}  // namespace browsing_data
