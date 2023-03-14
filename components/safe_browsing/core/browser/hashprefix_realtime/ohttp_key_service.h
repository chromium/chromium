// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_OHTTP_KEY_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_OHTTP_KEY_SERVICE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace safe_browsing {

// This class is responsible for managing the public key for sending Oblivious
// HTTP requests in hash real time lookup service.
class OhttpKeyService : public KeyedService {
 public:
  using Callback =
      base::OnceCallback<void(absl::optional<std::string> ohttp_key)>;

  OhttpKeyService();

  OhttpKeyService(const OhttpKeyService&) = delete;
  OhttpKeyService& operator=(const OhttpKeyService&) = delete;

  ~OhttpKeyService() override;

  // Gets an OHTTP key for encryption. It may or may not trigger a real time key
  // fetch, depending on whether there is a valid key already cached in memory
  // or there is an in-progress request triggered by other callers.
  // The key will be returned via |callback|. The callback runs with a nullopt
  // if the service cannot provide a valid key at the moment. This function is
  // overridden in tests.
  // TODO(crbug.com/1407283): Implement this function.
  virtual void GetOhttpKey(Callback callback);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_OHTTP_KEY_SERVICE_H_
