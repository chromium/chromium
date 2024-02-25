// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PRINTING_REMOTE_PPD_FETCHER_H_
#define CHROMEOS_PRINTING_REMOTE_PPD_FETCHER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "url/gurl.h"

namespace network::mojom {
class URLLoaderFactory;
}

namespace chromeos {

// A RemotePpdFetcher is used to fetch the contents of a PPD file hosted at an
// HTTP or HTTPS URL. The PPD file will then be used to configure a printer in
// CUPS.
class COMPONENT_EXPORT(CHROMEOS_PRINTING) RemotePpdFetcher {
 public:
  enum class FetchResultCode {
    kSuccess,

    // This is a catch-all status code for all network-related error, e.g. the
    // given URL was unreachable.
    kNetworkError,
  };
  using FetchCallback = base::OnceCallback<void(FetchResultCode, std::string)>;

  virtual ~RemotePpdFetcher() = default;

  // Fetch the contents of the given URL.
  //
  // `url` must be a valid URL with HTTP or HTTPS scheme.
  // If successful, `cb` will be invoked with FetchResult::kSuccess and the
  // contents of `url`. If unsuccessful, `cb` will be invoked with a FetchResult
  // indicating the error.
  virtual void Fetch(const GURL& url, FetchCallback cb) const = 0;

  // Create a `RemotePpdFetcher` instance.
  //
  // `loader_factory_dispenser` is a functor that can create fresh
  // URLLoaderFactory instances. We use this indirection to avoid
  // caching a raw pointer to a URLLoaderFactory instance, which is
  // invalidated by network service restarts.
  static std::unique_ptr<RemotePpdFetcher> Create(
      base::RepeatingCallback<network::mojom::URLLoaderFactory*()>
          loader_factory_dispenser);
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_REMOTE_PPD_FETCHER_H_
