// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/common/cors_exempt_headers.h"

#include "base/lazy_instance.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "chromecast/common/cors_exempt_headers.h"

namespace chromecast {

namespace {

constexpr char kAcceptLanguageHeader[] = "Accept-Language";

// Provides a list of CORS exempt headers for Cast Core.
class CorsExemptHeaders {
 public:
  CorsExemptHeaders() : cors_exempt_headers_({kAcceptLanguageHeader}) {
    base::ranges::copy(GetLegacyCorsExemptHeaders(),
                       std::back_inserter(cors_exempt_headers_));
  }

  // Returns the list of CORS exempt headers.
  const std::vector<std::string>& cors_exempt_headers() const {
    return cors_exempt_headers_;
  }

  // Checks if the header is CORS exempt using case-insensitive comparison.
  bool IsExempt(base::StringPiece header) const {
    return base::ranges::any_of(
        cors_exempt_headers_, [header](const std::string& cors_exempt_header) {
          return base::EqualsCaseInsensitiveASCII(cors_exempt_header, header);
        });
  }

 private:
  std::vector<std::string> cors_exempt_headers_;
};

base::LazyInstance<CorsExemptHeaders>::Leaky g_cors_exempt_headers =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

const std::vector<std::string>& GetCastCoreCorsExemptHeadersList() {
  return g_cors_exempt_headers.Get().cors_exempt_headers();
}

bool IsHeaderCorsExempt(base::StringPiece header) {
  return g_cors_exempt_headers.Get().IsExempt(header);
}

}  // namespace chromecast
