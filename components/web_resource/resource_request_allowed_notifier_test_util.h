// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_RESOURCE_RESOURCE_REQUEST_ALLOWED_NOTIFIER_TEST_UTIL_H_
#define COMPONENTS_WEB_RESOURCE_RESOURCE_REQUEST_ALLOWED_NOTIFIER_TEST_UTIL_H_

#include <memory>

#include "components/web_resource/resource_request_allowed_notifier.h"

class PrefService;

namespace web_resource {

// A subclass of ResourceRequestAllowedNotifier used to expose some
// functionality for testing.
//
// By default, the constructor sets this class to override
// ResourceRequestsAllowed, so its state can be set with SetRequestsAllowed.
// This is meant for higher level tests of services to ensure they adhere to the
// notifications of the ResourceRequestAllowedNotifier. Lower level tests can
// disable this by calling SetRequestsAllowedOverride with the value they want
// it to return.
class TestRequestAllowedNotifier : public ResourceRequestAllowedNotifier {
 public:
  TestRequestAllowedNotifier(
      PrefService* local_state,
      network::NetworkConnectionTracker* network_connection_tracker);

  TestRequestAllowedNotifier(const TestRequestAllowedNotifier&) = delete;
  TestRequestAllowedNotifier& operator=(const TestRequestAllowedNotifier&) =
      delete;

  ~TestRequestAllowedNotifier() override;

  // A version of |Init()| that accepts a custom EulaAcceptedNotifier.
  void InitWithEulaAcceptNotifier(
      Observer* observer,
      std::unique_ptr<EulaAcceptedNotifier> eula_notifier);

  // Makes ResourceRequestsAllowed return |allowed| when it is called.
  void SetRequestsAllowedOverride(bool allowed);

  // Notify observers that requests are allowed. This will only work if
  // the observer is expecting a notification.
  void NotifyObserver();

  // ResourceRequestAllowedNotifier overrides:
  State GetResourceRequestsAllowedState() override;
  EulaAcceptedNotifier* CreateEulaNotifier() override;

 private:
  std::unique_ptr<EulaAcceptedNotifier> test_eula_notifier_;
  bool override_requests_allowed_;
  bool requests_allowed_;
};

}  // namespace web_resource

#endif  // COMPONENTS_WEB_RESOURCE_RESOURCE_REQUEST_ALLOWED_NOTIFIER_TEST_UTIL_H_
