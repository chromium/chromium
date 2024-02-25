// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_INTERNET_AVAILABILITY_CHECKER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_INTERNET_AVAILABILITY_CHECKER_H_

namespace credential_provider {

// Checks the availability of an internet connection. In unit test this class
// can be overriden to return a forced value if desired.
class InternetAvailabilityChecker {
 public:
  static InternetAvailabilityChecker* Get();

  virtual bool HasInternetConnection();

  static void SetInstanceForTesting(InternetAvailabilityChecker*);

 protected:
  InternetAvailabilityChecker();
  virtual ~InternetAvailabilityChecker();

  // Returns the storage used for the instance pointer.
  static InternetAvailabilityChecker** GetInstanceStorage();
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_INTERNET_AVAILABILITY_CHECKER_H_
