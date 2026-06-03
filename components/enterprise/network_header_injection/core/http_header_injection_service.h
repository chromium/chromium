// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_HTTP_HEADER_INJECTION_SERVICE_H_
#define COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_HTTP_HEADER_INJECTION_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

class PrefService;

namespace enterprise_custom_headers {
class HttpHeaderInjectionMatcher;
}

namespace enterprise_custom_headers {

// A KeyedService that observes the HttpHeaderInjection policy preference
// and maintains the HttpHeaderInjectionMatcher.
class HttpHeaderInjectionService : public KeyedService {
 public:
  explicit HttpHeaderInjectionService(PrefService* prefs);
  HttpHeaderInjectionService(const HttpHeaderInjectionService&) = delete;
  HttpHeaderInjectionService& operator=(const HttpHeaderInjectionService&) =
      delete;
  ~HttpHeaderInjectionService() override;

  // Returns the headers that should be injected for the given URL.
  net::HttpRequestHeaders GetHeadersForUrl(const GURL& url) const;

  // Returns true if there are active rules in the matcher.
  bool HasRules() const;

  base::WeakPtr<HttpHeaderInjectionService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnPrefChanged();

  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<HttpHeaderInjectionMatcher> matcher_;

  base::WeakPtrFactory<HttpHeaderInjectionService> weak_ptr_factory_{this};
};

}  // namespace enterprise_custom_headers

#endif  // COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_HTTP_HEADER_INJECTION_SERVICE_H_
