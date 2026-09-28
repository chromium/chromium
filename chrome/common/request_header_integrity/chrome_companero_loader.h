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

// How often a cached token is refreshed, both by the child-process Mojo poll
// and by the browser-process feed in `ChromeCompaneroHost`.
inline constexpr base::TimeDelta kTokenRefreshInterval = base::Minutes(1);

// An in-memory request header integrity token cache, one per process.
//
// The cache is fed differently depending on the process:
// - Child processes (such as Renderers) call `SetMojoRemote()`, after which the
//   cache polls the remote `ChromeCompanero` service over Mojo every
//   `kTokenRefreshInterval`.
// - The browser process has no Mojo remote. `ChromeCompaneroHost` pushes tokens
//   in directly via `BrowserProcessUpdateCachedToken()`.
//
// `SetMojoRemote()` and the destructor must run on the same sequence.
// `GetHeaderNameAndValue()` and `BrowserProcessUpdateCachedToken()` may be
// called from any sequence.
class ChromeCompaneroLoader {
 public:
  static ChromeCompaneroLoader& GetInstance();

  ChromeCompaneroLoader(const ChromeCompaneroLoader&) = delete;
  ChromeCompaneroLoader& operator=(const ChromeCompaneroLoader&) = delete;

  // Returns the most recently received request header name and token value,
  // even if older than `kTokenRefreshInterval`, or std::nullopt if none has
  // been received. Non-blocking and thread-safe.
  std::optional<HeaderNameAndValue> GetHeaderNameAndValue();

  // Binds the Mojo remote to ChromeCompaneroHost. Triggers an initial token
  // refresh and arms the periodic refresh timer.
  void SetMojoRemote(
      mojo::PendingRemote<mojom::ChromeCompanero> pending_remote);

  // Replaces the cached request header name and token value. Used in the
  // browser process, which has no Mojo remote.
  void BrowserProcessUpdateCachedToken(std::string name, std::string value);

 protected:
  // Production code reaches the single instance through GetInstance(). Tests
  // that need an isolated instance to inject derive from this class instead of
  // constructing one directly.
  ChromeCompaneroLoader();
  ~ChromeCompaneroLoader();

  // Sets the cached header name and value without requiring IPC. Only
  // reachable from derived test helpers.
  void SetCacheForTesting(const std::string& name, const std::string& value);

 private:
  friend class base::NoDestructor<ChromeCompaneroLoader>;
  friend class ChromeCompaneroLoaderTest;

  void RefreshValue();
  void OnValueReceived(network::mojom::HttpRequestHeaderKeyValuePairPtr result);

  // Validates `name` and `value` and stores them in the cache.
  void SetCachedToken(std::string name, std::string value);

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
