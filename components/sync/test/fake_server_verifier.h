// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_VERIFIER_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_VERIFIER_H_

#include <stddef.h>

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/test/sessions_hierarchy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace fake_server {

class FakeServer;

// Provides methods to verify the state of a FakeServer. The main use case of
// this class is verifying committed data so that it does not have to be synced
// down to another test client for verification. These methods are not present
// on FakeServer so that its interface is not polluted.
class FakeServerVerifier {
 public:
  // Creates a FakeServerVerifier for |fake_server|. This class does not take
  // ownership of |fake_server|.
  explicit FakeServerVerifier(FakeServer* fake_server);

  FakeServerVerifier(const FakeServerVerifier&) = delete;
  FakeServerVerifier& operator=(const FakeServerVerifier&) = delete;

  virtual ~FakeServerVerifier();

  // Returns a successful result if there are |expected_count| entities with the
  // given |data_type|. A failure is returned if the count does not match or
  // verification can't take place.
  testing::AssertionResult VerifyEntityCountByType(
      size_t expected_count,
      syncer::DataType data_type) const;

  // Returns a successful result if there are |expected_count| entities with the
  // given |data_type| and |name|. A failure is returned if the count does not
  // match or verification can't take place.
  testing::AssertionResult VerifyEntityCountByTypeAndName(
      size_t expected_count,
      syncer::DataType data_type,
      const std::string& name) const;

  // Returns a successful result if |expected_sessions| matches the sessions
  // hierarchy present on the server. This method only supports one session.
  testing::AssertionResult VerifySessions(
      const SessionsHierarchy& expected_sessions);

  // Returns a successful result if |expected_urls| matches the URLs (within
  // DataType::HISTORY) on the server.
  testing::AssertionResult VerifyHistory(
      const std::multiset<GURL>& expected_urls);

 private:
  const raw_ptr<FakeServer> fake_server_;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_VERIFIER_H_
