// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_H_

#include <stddef.h>

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "components/signin/core/browser/signin_status_metrics_provider_base.h"
#include "components/signin/core/browser/signin_status_metrics_provider_delegate.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace metrics {
class ChromeUserMetricsExtension;
}

class SigninStatusMetricsProviderDelegate;

// Collect login status of all opened profiles during one UMA session and
// record the value into a histogram before UMA log is uploaded on platform
// Windows, Linux, Mac and Android.
class SigninStatusMetricsProvider : public SigninStatusMetricsProviderBase,
                                    public signin::IdentityManager::Observer {
 public:
  ~SigninStatusMetricsProvider() override;

  // SigninStatusMetricsProviderBase:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

  // Factory method, creates a new instance of this class.
  static std::unique_ptr<SigninStatusMetricsProvider> CreateInstance(
      std::unique_ptr<SigninStatusMetricsProviderDelegate> delegate);

  // Update the sign-in status when a IdentityManager is created.
  void OnIdentityManagerCreated(signin::IdentityManager* identity_manager);

  // Update the sign-in status when a IdentityManager is shut down.
  void OnIdentityManagerShutdown(signin::IdentityManager* identity_manager);

  // Updates the initial sign-in status. For testing purpose only.
  void UpdateInitialSigninStatusForTesting(size_t total_count,
                                           size_t signed_in_count);

  // Get the current recorded sign-in status. For testing purpose only.
  SigninStatus GetSigninStatusForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(SigninStatusMetricsProviderTest,
                           UpdateInitialSigninStatus);
  FRIEND_TEST_ALL_PREFIXES(SigninStatusMetricsProviderTest,
                           OnPrimaryAccountSet);
  FRIEND_TEST_ALL_PREFIXES(SigninStatusMetricsProviderTest,
                           OnPrimaryAccountCleared);

  // The boolean |is_test| indicates whether or not this is an instance for
  // testing purpose. If so, skip the initialization. Except for testing
  // purpose, this class's instance should be created through the static
  // CreateInstance() method.
  SigninStatusMetricsProvider(
      std::unique_ptr<SigninStatusMetricsProviderDelegate> delegate,
      bool is_test);

  // IdentityManager::Observer:
  void OnPrimaryAccountSet(const CoreAccountInfo& account_info) override;
  void OnPrimaryAccountCleared(const CoreAccountInfo& account_info) override;

  // Obtain sign-in status and add observers.
  void Initialize();

  // Update the sign-in status based on all currently opened profiles. Called by
  // ComputeCurrentSigninStatus at class construction and right after each UMA
  // log upload. |total_count| is the number of opened profiles and
  // |signed_in_count| represents the number of signed-in profiles among those
  // |total_count| profiles.
  void UpdateInitialSigninStatus(size_t total_count, size_t signed_in_count);

  // Compute current sign-in status of all opened profiles.
  void ComputeCurrentSigninStatus();

  std::unique_ptr<SigninStatusMetricsProviderDelegate> delegate_;

  // Used to track the IdentityManagers that this instance is observing so that
  // this instance can be removed as an observer on its destruction.
  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      scoped_observer_;

  // Whether the instance is for testing or not.
  bool is_test_;

  base::WeakPtrFactory<SigninStatusMetricsProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SigninStatusMetricsProvider);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_STATUS_METRICS_PROVIDER_H_
