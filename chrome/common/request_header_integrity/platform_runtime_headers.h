// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_REQUEST_HEADER_INTEGRITY_PLATFORM_RUNTIME_HEADERS_H_
#define CHROME_COMMON_REQUEST_HEADER_INTEGRITY_PLATFORM_RUNTIME_HEADERS_H_

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "net/http/http_request_headers.h"

namespace request_header_integrity {

// Outcome of applying the current headers to a request, for UMA.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PlatformRuntimeApplyResult {
  // No headers have been published to this process yet, so the request goes out
  // with whatever the throttle itself set.
  kUnavailable = 0,
  // Headers are available but the request carried none of them, so there was
  // nothing to overwrite.
  kNotApplicable = 1,
  // At least one header was modified.
  kApplied = 2,
  kMaxValue = kApplied,
};

// Process-local holder for the latest Platform Runtime headers.
//
// The Platform Runtime library runs only in a sandboxed utility process. The
// browser collects its output and pushes it to every renderer over
// chrome::mojom::RendererConfiguration.
// Both the browser and renderers then stamp requests from this holder, so no
// process other than the utility one ever executes the library and no network
// request ever blocks on IPC.
//
// Reads happen on arbitrary request threads, so access is lock-guarded.
class PlatformRuntimeHeaders {
 public:
  static PlatformRuntimeHeaders& GetInstance();

  PlatformRuntimeHeaders(const PlatformRuntimeHeaders&) = delete;
  PlatformRuntimeHeaders& operator=(const PlatformRuntimeHeaders&) = delete;

  // Replaces the current headers. An empty or unusable `headers` clears the
  // holder.
  void Set(const net::HttpRequestHeaders& headers);

  // Overwrites those headers in `headers` for which the library produced a
  // value.
  PlatformRuntimeApplyResult Apply(net::HttpRequestHeaders* headers) const;

  void ResetForTesting();

 private:
  friend class base::NoDestructor<PlatformRuntimeHeaders>;

  PlatformRuntimeHeaders();
  ~PlatformRuntimeHeaders();

  mutable base::Lock lock_;
  net::HttpRequestHeaders headers_ GUARDED_BY(lock_);
};

}  // namespace request_header_integrity

#endif  // CHROME_COMMON_REQUEST_HEADER_INTEGRITY_PLATFORM_RUNTIME_HEADERS_H_
