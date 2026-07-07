// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/request_header_integrity/chrome_companero_loader.h"

#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/scoped_thread_priority.h"
#include "build/branding_buildflags.h"
#include "chrome/common/request_header_integrity/chrome_companero.mojom.h"
#include "components/embedder_support/user_agent_utils.h"
#include "content/public/common/content_switches.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/http_request_headers.mojom.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/common/request_header_integrity/internal/integrity_seed_internal.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#endif

namespace request_header_integrity {

// static
ChromeCompaneroLoader& ChromeCompaneroLoader::GetInstance() {
  static base::NoDestructor<ChromeCompaneroLoader> instance;
  return *instance;
}

#if BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY)

namespace {

base::FilePath GetLibraryPath() {
#if BUILDFLAG(IS_MAC)
  base::FilePath framework_dir = base::apple::FrameworkBundlePath();
  return framework_dir.Append(FILE_PATH_LITERAL("Libraries"))
      .Append(FILE_PATH_LITERAL("libchromecompaneros.dylib"));
#else
  base::FilePath module_dir;
  if (!base::PathService::Get(base::DIR_MODULE, &module_dir)) {
    return base::FilePath();
  }
#if BUILDFLAG(IS_WIN)
  return module_dir.Append(FILE_PATH_LITERAL("chromecompaneros.dll"));
#else
  return module_dir.Append(FILE_PATH_LITERAL("libchromecompaneros.so"));
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(IS_MAC)
}

constexpr base::TimeDelta kRefreshInterval = base::Minutes(1);

}  // namespace

ChromeCompaneroLoader::ChromeCompaneroLoader()
    : refresh_timer_(FROM_HERE,
                     kRefreshInterval,
                     base::BindRepeating(&ChromeCompaneroLoader::RefreshValue,
                                         base::Unretained(this))) {}

ChromeCompaneroLoader::~ChromeCompaneroLoader() = default;

std::optional<std::string> ChromeCompaneroLoader::GetHeaderValueFromLib(
    std::string_view seed,
    std::string_view api_key,
    std::string_view user_agent) {
  if (!init_succeeded_.load(std::memory_order_acquire)) {
    return std::nullopt;
  }

  char value_buffer[64];
  int32_t written = get_companero_value_fn_(
      seed.data(), seed.length(), api_key.data(), api_key.length(),
      user_agent.data(), user_agent.length(), value_buffer,
      sizeof(value_buffer));
  if (written <= 0) {
    return std::nullopt;
  }
  CHECK_LE(static_cast<size_t>(written), sizeof(value_buffer));
  return std::string(value_buffer, static_cast<size_t>(written));
}

void ChromeCompaneroLoader::SetMojoRemote(
    mojo::PendingRemote<mojom::ChromeCompanero> pending_remote) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessType));
  base::AutoLock lock(cache_lock_);
  companero_remote_ =
      mojo::SharedRemote<mojom::ChromeCompanero>(std::move(pending_remote));
  RefreshValueLocked();
}

void ChromeCompaneroLoader::RefreshValue() {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessType));
  base::AutoLock lock(cache_lock_);
  RefreshValueLocked();
}

void ChromeCompaneroLoader::RefreshValueLocked() {
  refresh_timer_.Reset();
  if (!companero_remote_.is_bound()) {
    return;
  }

  companero_remote_->GetHeaderNameAndValue(base::BindOnce(
      &ChromeCompaneroLoader::OnValueReceived, base::Unretained(this)));
}

void ChromeCompaneroLoader::OnValueReceived(
    network::mojom::HttpRequestHeaderKeyValuePairPtr result) {
  if (!result) {
    return;
  }
  CHECK(net::HttpUtil::IsValidHeaderName(result->key));
  CHECK(net::HttpUtil::IsValidHeaderValue(result->value));
  base::AutoLock lock(cache_lock_);
  if (cached_header_name_.empty()) {
    cached_header_name_ = std::move(result->key);
  } else {
    CHECK_EQ(result->key, cached_header_name_);
  }
  cached_value_ = std::move(result->value);
  // Note: Recording Now() upon IPC receipt may extend a cached token's
  // effective TTL in child processes.
  cached_value_time_ = base::TimeTicks::Now();
}

void ChromeCompaneroLoader::BrowserProcessInitialize() {
  CHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessType));

  bool expected = false;
  CHECK(init_called_.compare_exchange_strong(expected, true,
                                             std::memory_order_acq_rel));

  base::FilePath library_path = GetLibraryPath();
  if (library_path.empty()) {
    return;
  }

  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  library_ = base::ScopedNativeLibrary(library_path);
  if (!library_.is_valid()) {
    VLOG(1) << "Failed to load companeros library: "
            << (library_.GetError() ? library_.GetError()->ToString()
                                    : "Unknown error");
    return;
  }

  get_companero_value_fn_ = reinterpret_cast<GetCompaneroValueFunc>(
      library_.GetFunctionPointer("GetCompaneroValue"));
  get_header_name_fn_ = reinterpret_cast<GetHeaderNameFunc>(
      library_.GetFunctionPointer("GetHeaderName"));

  if (!get_companero_value_fn_ || !get_header_name_fn_) {
    get_companero_value_fn_ = nullptr;
    get_header_name_fn_ = nullptr;
    library_.reset();
    return;
  }

  init_succeeded_.store(true, std::memory_order_release);
}

std::optional<std::string> ChromeCompaneroLoader::GetHeaderNameFromLib() {
  CHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessType));

  if (!init_succeeded_.load(std::memory_order_acquire)) {
    return std::nullopt;
  }

  char name_buffer[64];
  int32_t written = get_header_name_fn_(name_buffer, sizeof(name_buffer));
  if (written <= 0) {
    return std::nullopt;
  }
  CHECK_LE(static_cast<size_t>(written), sizeof(name_buffer));
  return std::string(name_buffer, static_cast<size_t>(written));
}

std::optional<HeaderNameAndValue>
ChromeCompaneroLoader::GetHeaderNameAndValue() {
  base::AutoLock lock(cache_lock_);
  constexpr base::TimeDelta kCacheTtl = base::Minutes(2);
  if (!cached_value_time_.is_null() &&
      (base::TimeTicks::Now() - cached_value_time_ < kCacheTtl)) {
    return HeaderNameAndValue{cached_header_name_, cached_value_};
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kProcessType)) {
    // Note: The header name is fixed for the lifetime of the browser process,
    // so cached_header_name_ is checked independently of token expiration.
    if (cached_header_name_.empty()) {
      std::optional<std::string> resolved_name = GetHeaderNameFromLib();
      if (!resolved_name) {
        return std::nullopt;
      }
      CHECK(net::HttpUtil::IsValidHeaderName(*resolved_name));
      cached_header_name_ = *std::move(resolved_name);
    }

    std::string_view seed;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    seed = kIntegritySeed;
#endif
    std::optional<std::string> value = GetHeaderValueFromLib(
        seed, google_apis::GetAPIKey(), embedder_support::GetUserAgent());
    if (value) {
      CHECK(net::HttpUtil::IsValidHeaderValue(*value));
      cached_value_ = *std::move(value);
      cached_value_time_ = base::TimeTicks::Now();
      return HeaderNameAndValue{cached_header_name_, cached_value_};
    }
  }

  // Fallback: If DSO lookup failed or if in child process, return stale cache
  // if available.
  if (!cached_header_name_.empty() && !cached_value_.empty()) {
    return HeaderNameAndValue{cached_header_name_, cached_value_};
  }

  return std::nullopt;
}

#else  // BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY)

ChromeCompaneroLoader::ChromeCompaneroLoader() = default;
ChromeCompaneroLoader::~ChromeCompaneroLoader() = default;

void ChromeCompaneroLoader::BrowserProcessInitialize() {}

std::optional<HeaderNameAndValue>
ChromeCompaneroLoader::GetHeaderNameAndValue() {
  return std::nullopt;
}

void ChromeCompaneroLoader::SetMojoRemote(
    mojo::PendingRemote<mojom::ChromeCompanero> pending_remote) {}

#endif  // BUILDFLAG(ENABLE_REQUEST_HEADER_INTEGRITY)

}  // namespace request_header_integrity
