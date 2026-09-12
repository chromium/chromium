// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_LOADER_H_
#define CHROME_COMMON_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_LOADER_H_

#include <optional>
#include <string>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/common/request_header_integrity/chrome_companero.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/http_request_headers.mojom-forward.h"

namespace request_header_integrity {

struct HeaderNameAndValue {
  std::string name;
  std::string value;
};

// An in-memory token cache for child processes (such as Renderers).
//
// Periodically polls a remote `ChromeCompanero` service over Mojo for request
// header integrity tokens.
//
// Must be created, configured with a `PendingRemote`, and destroyed on the same
// sequence. Its `GetHeaderNameAndValue()` method may be called from any
// sequence.
class ChromeCompaneroLoader {
 public:
  static ChromeCompaneroLoader& GetInstance();

  ChromeCompaneroLoader(const ChromeCompaneroLoader&) = delete;
  ChromeCompaneroLoader& operator=(const ChromeCompaneroLoader&) = delete;

  // Returns the cached request header name and token value from RAM, or
  // std::nullopt if the token is unavailable or uninitialized. Non-blocking
  // and thread-safe.
  std::optional<HeaderNameAndValue> GetHeaderNameAndValue();

  // Binds the Mojo remote to ChromeCompaneroHost. Triggers an initial token
  // refresh and arms the periodic refresh timer.
  void SetMojoRemote(
      mojo::PendingRemote<mojom::ChromeCompanero> pending_remote);

 private:
  friend class base::NoDestructor<ChromeCompaneroLoader>;
  friend class ChromeCompaneroLoaderTest;

  ChromeCompaneroLoader();
  ~ChromeCompaneroLoader();

  void RefreshValue();
  void OnValueReceived(network::mojom::HttpRequestHeaderKeyValuePairPtr result);

  // Protects access to the cached token fields below. Not held during blocking
  // operations.
  base::Lock cache_lock_;
  std::string cached_header_name_ GUARDED_BY(cache_lock_);
  std::string cached_value_ GUARDED_BY(cache_lock_);
  base::TimeTicks cached_value_time_ GUARDED_BY(cache_lock_);

  // Sequence-bound members for Mojo IPC and periodic polling.
  mojo::Remote<mojom::ChromeCompanero> companero_remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // TODO(deepakr): Waking up background renderers periodically just to
  // refresh the token is inefficient. Consider switching to a lazy,
  // on-demand refresh model where a new token is only requested from the
  // browser when a network request is actually initiated and the cached
  // token needs to be refreshed.
  base::RetainingOneShotTimer refresh_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace request_header_integrity

#endif  // CHROME_COMMON_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_LOADER_H_
