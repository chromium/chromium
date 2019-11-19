// Copyright 2019 The Chromium Authors. All rights reserved.
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

namespace password_manager {

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

  // The leak check is available only for signed-in users and if the feature is
  // available.
  // |delegate| gets the results for the fetch.
  // |identity_manager| is used to obtain the token.
  // |url_loader_factory| does the actual network request.
  virtual std::unique_ptr<LeakDetectionCheck> TryCreateLeakCheck(
      LeakDetectionDelegateInterface* delegate,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      const = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_CHECK_FACTORY_H_
