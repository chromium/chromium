// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ERROR_PAGE_COMMON_ERROR_H_
#define COMPONENTS_ERROR_PAGE_COMMON_ERROR_H_

#include <string>

#include "net/dns/public/resolve_error_info.h"
#include "url/gurl.h"

namespace error_page {

enum LinkPreviewErrorCode {
  // Non HTTPS URL is not allowed to preview.
  kNonHttpsForbidden,
};

// Represents an error info necessary to show an error page.
// This class is a copiable value class.
class Error {
 public:
  // For network errors
  static const char kNetErrorDomain[];
  // For http errors.
  static const char kHttpErrorDomain[];
  // For DNS probe errors.
  static const char kDnsProbeErrorDomain[];
  // For Link Preview errors.
  static const char kLinkPreviewErrorDomain[];

  // Returns a kNetErrorDomain error.
  static Error NetError(const GURL& url,
                        int reason,
                        int extended_reason,
                        net::ResolveErrorInfo resolve_error_info,
                        bool stale_copy_in_cache);
  // Returns a kHttpErrorDomain error.
  static Error HttpError(const GURL& url, int status);
  // Returns a kDnsProbeErrorDomain error.
  static Error DnsProbeError(const GURL& url,
                             int status,
                             bool stale_copy_in_cache);
  // Returns a kLinkPreviewErrorDomain error.
  static Error LinkPreviewError(const GURL& url,
                                LinkPreviewErrorCode error_code);

  ~Error();
  Error(const Error&);
  Error& operator=(const Error&);

  // Returns the url that failed to load.
  const GURL& url() const { return url_; }
  // Returns the domain of this error.
  const std::string& domain() const { return domain_; }
  // Returns a numeric error code. The meaning of this code depends on the
  // domain string.
  int reason() const { return reason_; }
  // Returns a numeric error code containing additional information about the
  // error. Note that the extended reason is only relevant when `reason()` is
  // `kNetErrorDomain`.
  int extended_reason() const { return extended_reason_; }
  // Returns error details of the host resolution.
  const net::ResolveErrorInfo& resolve_error_info() const {
    return resolve_error_info_;
  }
  // Returns true if chrome has a stale cache entry for the url.
  bool stale_copy_in_cache() const { return stale_copy_in_cache_; }

 private:
  Error(const GURL& url,
        const std::string& domain,
        int reason,
        int extended_reason,
        net::ResolveErrorInfo resolve_error_info,
        bool stale_copy_in_cache);

  GURL url_;
  std::string domain_;
  int reason_;
  int extended_reason_;
  net::ResolveErrorInfo resolve_error_info_;
  bool stale_copy_in_cache_;
};

}  // namespace error_page

#endif  // COMPONENTS_ERROR_PAGE_COMMON_ERROR_H_
