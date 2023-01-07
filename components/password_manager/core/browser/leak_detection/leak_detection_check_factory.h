// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_FACTORY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace version_info {
enum class Channel;
}

namespace password_manager {

class BulkLeakCheck;
class BulkLeakCheckDelegateInterface;
class LeakDetectionCheck;
class LeakDetectionDelegateInterface;

// The interface for creating instances of requests for checking if
// {username, password} pair was leaked in the internet.
class LeakDetectionCheckFactory {
 public:
  LeakDetectionCheckFactory() = default;
  virtual ~LeakDetectionCheckFactory() = default;

  // Not copyable or movable
  LeakDetectionCheckFactory(const LeakDetectionCheckFactory&) = delete;
  LeakDetectionCheckFactory& operator=(const LeakDetectionCheckFactory&) =
      delete;
  LeakDetectionCheckFactory(LeakDetectionCheckFactory&&) = delete;
  LeakDetectionCheckFactory& operator=(LeakDetectionCheckFactory&&) = delete;

  // The leak check is available for all signed-in users. If the feature is
  // enabled, it is also available for signed-out users.
  // |delegate| gets the results for the fetch.
  // |identity_manager| is used to obtain the token for signed in users.
  // |url_loader_factory| does the actual network request.
  // |channel| is used to obtain correct api key for signed out users.
  virtual std::unique_ptr<LeakDetectionCheck> TryCreateLeakCheck(
      LeakDetectionDelegateInterface* delegate,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      version_info::Channel channel) const = 0;

  // The leak check is available only for signed-in users and if the feature is
  // available.
  // |delegate| gets the results for the fetch.
  // |identity_manager| is used to obtain the token.
  // |url_loader_factory| does the actual network request.
  virtual std::unique_ptr<BulkLeakCheck> TryCreateBulkLeakCheck(
      BulkLeakCheckDelegateInterface* delegate,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      const = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_FACTORY_H_
