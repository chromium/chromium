// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_REQUIREMENTS_SPEC_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_REQUIREMENTS_SPEC_FETCHER_H_

#include "base/callback.h"
#include "url/gurl.h"

namespace autofill {

class PasswordRequirementsSpec;

// Fetches PasswordRequirementsSpec for a specific origin.
class PasswordRequirementsSpecFetcher {
 public:
  using FetchCallback =
      base::OnceCallback<void(const PasswordRequirementsSpec&)>;

  virtual ~PasswordRequirementsSpecFetcher() = default;

  // Fetches a configuration for |origin|.
  //
  // |origin| references the origin in the PasswordForm for which rules need to
  // be fetched.
  //
  // The |callback| must remain valid until called back, but this class may be
  // destroyed before the |callback| has been triggered.
  //
  // Fetch() may be called multiple times concurrently. Requests are batched
  // if possible.
  //
  // If the network request fails or times out, the callback receives an empty
  // spec.
  virtual void Fetch(GURL origin, FetchCallback callback) = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GENERATION_PASSWORD_REQUIREMENTS_SPEC_FETCHER_H_
