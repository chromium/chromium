// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/request_header_integrity/chrome_companero_loader.h"

#include <utility>

#include "base/check_op.h"
#include "base/command_line.h"
#include "chrome/common/request_header_integrity/chrome_companero.mojom.h"
#include "content/public/common/content_switches.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/http_request_headers.mojom.h"

namespace request_header_integrity {

// static
ChromeCompaneroLoader& ChromeCompaneroLoader::GetInstance() {
  static base::NoDestructor<ChromeCompaneroLoader> instance;
  return *instance;
}

ChromeCompaneroLoader::ChromeCompaneroLoader()
    : refresh_timer_(FROM_HERE,
                     kTokenRefreshInterval,
                     base::BindRepeating(&ChromeCompaneroLoader::RefreshValue,
                                         base::Unretained(this))) {}

ChromeCompaneroLoader::~ChromeCompaneroLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ChromeCompaneroLoader::SetMojoRemote(
    mojo::PendingRemote<mojom::ChromeCompanero> pending_remote) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessType));
  companero_remote_.Bind(std::move(pending_remote));
  RefreshValue();
}

void ChromeCompaneroLoader::BrowserProcessUpdateCachedToken(std::string name,
                                                            std::string value) {
  CHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessType));
  SetCachedToken(std::move(name), std::move(value));
}

void ChromeCompaneroLoader::RefreshValue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessType));
  refresh_timer_.Reset();
  if (!companero_remote_.is_bound()) {
    return;
  }

  companero_remote_->GetHeaderNameAndValue(base::BindOnce(
      &ChromeCompaneroLoader::OnValueReceived, base::Unretained(this)));
}

void ChromeCompaneroLoader::OnValueReceived(
    network::mojom::HttpRequestHeaderKeyValuePairPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result) {
    return;
  }
  // Note: Recording Now() upon IPC receipt may extend a cached token's
  // effective TTL in child processes.
  SetCachedToken(std::move(result->key), std::move(result->value));
}

void ChromeCompaneroLoader::SetCachedToken(std::string name,
                                           std::string value) {
  CHECK(net::HttpUtil::IsValidHeaderName(name));
  CHECK(net::HttpUtil::IsValidHeaderValue(value));
  base::AutoLock lock(cache_lock_);
  if (cached_header_name_.empty()) {
    cached_header_name_ = std::move(name);
  } else {
    CHECK_EQ(name, cached_header_name_);
  }
  cached_value_ = std::move(value);
  cached_value_time_ = base::TimeTicks::Now();
}

std::optional<HeaderNameAndValue>
ChromeCompaneroLoader::GetHeaderNameAndValue() {
  base::AutoLock lock(cache_lock_);
  constexpr base::TimeDelta kCacheTtl = base::Minutes(2);
  if (!cached_value_time_.is_null() &&
      (base::TimeTicks::Now() - cached_value_time_ < kCacheTtl)) {
    return HeaderNameAndValue{cached_header_name_, cached_value_};
  }

  // Fallback: Return stale cache if available.
  if (!cached_header_name_.empty() && !cached_value_.empty()) {
    return HeaderNameAndValue{cached_header_name_, cached_value_};
  }

  return std::nullopt;
}

void ChromeCompaneroLoader::SetCacheForTesting(const std::string& name,
                                               const std::string& value) {
  base::AutoLock lock(cache_lock_);
  cached_header_name_ = name;
  cached_value_ = value;
  cached_value_time_ = name.empty() && value.empty() ? base::TimeTicks()
                                                     : base::TimeTicks::Now();
}

}  // namespace request_header_integrity
