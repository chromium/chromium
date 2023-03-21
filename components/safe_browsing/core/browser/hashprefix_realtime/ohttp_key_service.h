// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_OHTTP_KEY_SERVICE_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_OHTTP_KEY_SERVICE_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace safe_browsing {

// This class is responsible for managing the public key for sending Oblivious
// HTTP requests in hash real time lookup service.
class OhttpKeyService : public KeyedService {
 public:
  using Callback =
      base::OnceCallback<void(absl::optional<std::string> ohttp_key)>;

  explicit OhttpKeyService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  OhttpKeyService(const OhttpKeyService&) = delete;
  OhttpKeyService& operator=(const OhttpKeyService&) = delete;

  ~OhttpKeyService() override;

  // Gets an OHTTP key for encryption. It may or may not trigger a real time key
  // fetch, depending on whether there is a valid key already cached in memory
  // or there is an in-progress request triggered by other callers.
  // The key will be returned via |callback|. The callback runs with a nullopt
  // if the service cannot provide a valid key at the moment. Callers should
  // ensure |callback| is still valid when it is run. This function is
  // overridden in tests.
  virtual void GetOhttpKey(Callback callback);

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

 private:
  // Called when the response from the Safe Browsing key hosting endpoint is
  // received.
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  // The URLLoaderFactory we use to issue a network request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // |url_loader_| is not null iff there is a network request in progress.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // All callbacks that have requested an OHTTP key but haven't received a
  // response yet.
  base::OnceCallbackList<Callback::RunType> pending_callbacks_;

  base::WeakPtrFactory<OhttpKeyService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_HASHPREFIX_REALTIME_OHTTP_KEY_SERVICE_H_
