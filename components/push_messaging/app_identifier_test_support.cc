// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/push_messaging/app_identifier_test_support.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace push_messaging {

// static
AppIdentifier AppIdentifierTestSupport::GenerateId(
    const GURL& origin,
    int64_t service_worker_registration_id) {
  // To bypass DCHECK in AppIdentifier::Generate, we just use
  // it to generate app_id, and then use private constructor.
  std::string app_id =
      AppIdentifier::Generate(GURL("https://www.example.com/"), 1).app_id();
  return AppIdentifier(app_id, origin, service_worker_registration_id);
}

// static
void AppIdentifierTestSupport::ExpectAppIdentifiersEqual(
    const AppIdentifier& a,
    const AppIdentifier& b) {
  EXPECT_EQ(a.app_id(), b.app_id());
  EXPECT_EQ(a.origin(), b.origin());
  EXPECT_EQ(a.service_worker_registration_id(),
            b.service_worker_registration_id());
  EXPECT_EQ(a.expiration_time(), b.expiration_time());
}

// static
AppIdentifier AppIdentifierTestSupport::ReplaceAppId(
    const AppIdentifier& origin,
    std::string_view app_id) {
  AppIdentifier copy = origin;
  copy.app_id_ = app_id;
  return copy;
}

// static
AppIdentifier AppIdentifierTestSupport::LegacyGenerateForTesting(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::optional<base::Time>& expiration_time /* = std::nullopt */) {
  return AppIdentifier::GenerateInternal(origin, service_worker_registration_id,
                                         false /* use_instance_id */,
                                         expiration_time);
}

}  // namespace push_messaging
