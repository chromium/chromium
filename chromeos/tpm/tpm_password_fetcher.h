// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_TPM_TPM_PASSWORD_FETCHER_H_
#define CHROMEOS_TPM_TPM_PASSWORD_FETCHER_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"

namespace chromeos {

// Interface which TpmPasswordFetcher uses to notify that password has been
// fetched.
class COMPONENT_EXPORT(CHROMEOS_TPM) TpmPasswordFetcherDelegate {
 public:
  virtual ~TpmPasswordFetcherDelegate() {}
  virtual void OnPasswordFetched(const std::string& tpm_password) = 0;
};

// Class for fetching TPM password from the Cryptohome.
class COMPONENT_EXPORT(CHROMEOS_TPM) TpmPasswordFetcher {
 public:
  // Creates fetcher with the given delegate to be notified every time fetching
  // is done.
  explicit TpmPasswordFetcher(TpmPasswordFetcherDelegate* delegate);
  ~TpmPasswordFetcher();

  // Fetches TPM password and stores the result. Also notifies |delegate_| with
  // OnPasswordFetched() call.
  void Fetch();

 private:
  // Used to implement Fetch().
  void OnTpmIsReady(base::Optional<bool> tpm_is_ready);

  // Used to implement Fetch().
  void OnTpmGetPassword(base::Optional<std::string> password);

  // Posts a task to call Fetch() later.
  void RescheduleFetch();

  TpmPasswordFetcherDelegate* delegate_;

  base::WeakPtrFactory<TpmPasswordFetcher> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TpmPasswordFetcher);
};

}  // namespace chromeos

#endif  // CHROMEOS_TPM_TPM_PASSWORD_FETCHER_H_
