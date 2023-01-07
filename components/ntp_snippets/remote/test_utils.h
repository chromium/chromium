// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_TEST_UTILS_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_TEST_UTILS_H_

#include <memory>

#include "components/sync_preferences/testing_pref_service_syncable.h"

using sync_preferences::TestingPrefServiceSyncable;

namespace ntp_snippets {

namespace test {

// Common utilities for remote suggestion tests, handles initializing fakes.
class RemoteSuggestionsTestUtils {
 public:
  RemoteSuggestionsTestUtils();
  ~RemoteSuggestionsTestUtils();

  TestingPrefServiceSyncable* pref_service() { return pref_service_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSyncable> pref_service_;
};

}  // namespace test

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_TEST_UTILS_H_
