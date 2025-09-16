// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PUSH_MESSAGING_APP_IDENTIFIER_TEST_SUPPORT_H_
#define COMPONENTS_PUSH_MESSAGING_APP_IDENTIFIER_TEST_SUPPORT_H_

#include <optional>
#include <string_view>

#include "base/time/time.h"
#include "components/push_messaging/app_identifier.h"

class GURL;

namespace push_messaging {

// Set of functions commonly used in AppIdentifier related tests which needs the
// access to the internal state of AppIdentifier.
class AppIdentifierTestSupport {
 protected:
  static AppIdentifier GenerateId(const GURL& origin,
                                  int64_t service_worker_registration_id);

  static void ExpectAppIdentifiersEqual(const AppIdentifier& a,
                                        const AppIdentifier& b);

  // Creates a new AppIdentifier same to the |origin| but with a different
  // |app_id| field.
  static AppIdentifier ReplaceAppId(const AppIdentifier& origin,
                                    std::string_view app_id);

  // Generates a new app identifier for legacy GCM (not modern InstanceID).
  // Allows the derived class creating a legacy instance for tests.
  static AppIdentifier LegacyGenerateForTesting(
      const GURL& origin,
      int64_t service_worker_registration_id,
      const std::optional<base::Time>& expiration_time = std::nullopt);
};

}  // namespace push_messaging

#endif  // COMPONENTS_PUSH_MESSAGING_APP_IDENTIFIER_TEST_SUPPORT_H_
