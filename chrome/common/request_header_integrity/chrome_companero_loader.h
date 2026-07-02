// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_LOADER_H_
#define CHROME_COMMON_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_LOADER_H_

#include <stdint.h>

#include <atomic>
#include <optional>
#include <string>
#include <string_view>

#include "base/no_destructor.h"
#include "chrome/common/request_header_integrity/buildflags.h"
#include "chrome/common/request_header_integrity/chrome_companero.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

#if BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY)
#include "base/scoped_native_library.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#endif  // BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY)

namespace request_header_integrity {

struct HeaderNameAndValue {
  std::string name;
  std::string value;
};

class ChromeCompaneroLoader {
 public:
  static ChromeCompaneroLoader& GetInstance();

  ChromeCompaneroLoader(const ChromeCompaneroLoader&) = delete;
  ChromeCompaneroLoader& operator=(const ChromeCompaneroLoader&) = delete;

  // Loads the dynamic library and resolves C function pointers. Must be
  // called exactly once during early Browser process startup from a thread
  // that allows blocking disk I/O.
  void BrowserProcessInitialize();

  // Returns the cached request header name and token value.
  // In child processes (Renderers), lookups strictly return non-blocking cached
  // RAM entries. In the browser process, cache misses fall back to on-demand
  // dynamic library extraction.
  std::optional<HeaderNameAndValue> GetHeaderNameAndValue();

  void SetMojoRemote(
      mojo::PendingRemote<mojom::ChromeCompanero> pending_remote);

 protected:
  ChromeCompaneroLoader();
  ~ChromeCompaneroLoader();

 private:
  friend class base::NoDestructor<ChromeCompaneroLoader>;
  friend class ChromeCompaneroLoaderTest;

#if BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY)
  // Retrieves the header name from the loaded dynamic library.
  // Stateless helper that extracts the raw header name from the DSO.
  std::optional<std::string> GetHeaderNameFromLib();

  // Retrieves the token value from the loaded dynamic library.
  // Stateless helper that extracts a fresh token value from the DSO.
  std::optional<std::string> GetHeaderValueFromLib(std::string_view seed,
                                                   std::string_view api_key,
                                                   std::string_view user_agent);

  // TODO(deepakr): Update FFI return types from int32_t to size_t to cleanly
  // represent written byte counts without signed conversion artifacts across
  // ABI boundaries.
  // Typedefs for function pointers.
  using GetCompaneroValueFunc = int32_t (*)(const char*,
                                            size_t,
                                            const char*,
                                            size_t,
                                            const char*,
                                            size_t,
                                            char*,
                                            size_t);
  using GetHeaderNameFunc = int32_t (*)(char*, size_t);

  void RefreshValue();
  void RefreshValueLocked() EXCLUSIVE_LOCKS_REQUIRED(cache_lock_);
  void OnValueReceived(mojom::HeaderNameAndValuePtr result);

  base::ScopedNativeLibrary library_;
  GetCompaneroValueFunc get_companero_value_fn_ = nullptr;
  GetHeaderNameFunc get_header_name_fn_ = nullptr;
  std::atomic_bool init_called_{false};
  std::atomic_bool init_succeeded_{false};

  base::Lock cache_lock_;
  std::string cached_header_name_ GUARDED_BY(cache_lock_);
  std::string cached_value_ GUARDED_BY(cache_lock_);
  base::TimeTicks cached_value_time_ GUARDED_BY(cache_lock_);
  mojo::SharedRemote<mojom::ChromeCompanero> companero_remote_
      GUARDED_BY(cache_lock_);

  // TODO(deepakr): Waking up background renderers periodically just to
  // refresh the token is inefficient. Consider switching to a lazy,
  // on-demand refresh model where we only request a new token from the
  // browser when a network request is actually initiated and the cached
  // token needs to be refreshed.
  base::RetainingOneShotTimer refresh_timer_ GUARDED_BY(cache_lock_);
#endif  // BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY)
};

}  // namespace request_header_integrity

#endif  // CHROME_COMMON_REQUEST_HEADER_INTEGRITY_CHROME_COMPANERO_LOADER_H_
