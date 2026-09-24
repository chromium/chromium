// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/request_header_integrity/platform_runtime_headers.h"

#include <utility>

#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

namespace request_header_integrity {

// static
PlatformRuntimeHeaders& PlatformRuntimeHeaders::GetInstance() {
  static base::NoDestructor<PlatformRuntimeHeaders> instance;
  return *instance;
}

PlatformRuntimeHeaders::PlatformRuntimeHeaders() = default;
PlatformRuntimeHeaders::~PlatformRuntimeHeaders() = default;

void PlatformRuntimeHeaders::Set(const net::HttpRequestHeaders& headers) {
  net::HttpRequestHeaders valid_headers;
  for (const auto& header : headers.GetHeaderVector()) {
    if (!net::HttpUtil::IsValidHeaderName(header.key) ||
        !net::HttpUtil::IsValidHeaderValue(header.value)) {
      continue;
    }
    valid_headers.SetHeader(header.key, header.value);
  }

  base::AutoLock lock(lock_);
  headers_ = std::move(valid_headers);
}

PlatformRuntimeApplyResult PlatformRuntimeHeaders::Apply(
    net::HttpRequestHeaders* headers) const {
  base::AutoLock lock(lock_);
  if (headers_.IsEmpty()) {
    return PlatformRuntimeApplyResult::kUnavailable;
  }
  if (!headers) {
    return PlatformRuntimeApplyResult::kNotApplicable;
  }

  bool applied = false;
  for (const auto& header : headers_.GetHeaderVector()) {
    if (headers->HasHeader(header.key)) {
      headers->SetHeader(header.key, header.value);
      applied = true;
    }
  }
  return applied ? PlatformRuntimeApplyResult::kApplied
                 : PlatformRuntimeApplyResult::kNotApplicable;
}

void PlatformRuntimeHeaders::ResetForTesting() {
  base::AutoLock lock(lock_);
  headers_.Clear();
}

}  // namespace request_header_integrity
