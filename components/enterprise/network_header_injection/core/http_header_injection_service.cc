// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/network_header_injection/core/http_header_injection_service.h"

#include "base/functional/bind.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_matcher.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_rule.h"
#include "components/enterprise/network_header_injection/core/network_header_injection_prefs.h"
#include "components/prefs/pref_service.h"

namespace enterprise_custom_headers {

HttpHeaderInjectionService::HttpHeaderInjectionService(PrefService* prefs)
    : matcher_(HttpHeaderInjectionMatcher::Create()) {
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kHttpHeaderInjection,
      base::BindRepeating(&HttpHeaderInjectionService::OnPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  OnPrefChanged();  // Load rules initially.
}

HttpHeaderInjectionService::~HttpHeaderInjectionService() = default;

void HttpHeaderInjectionService::OnPrefChanged() {
  const auto& policy_list =
      pref_change_registrar_.prefs()->GetList(prefs::kHttpHeaderInjection);

  std::vector<HttpHeaderInjectionRule> rules;
  for (const auto& value : policy_list) {
    auto rule = HttpHeaderInjectionRule::FromValue(value);
    if (rule.has_value()) {
      rules.push_back(std::move(*rule));
    }
  }

  matcher_->UpdateRules(std::move(rules));
}

net::HttpRequestHeaders HttpHeaderInjectionService::GetHeadersForUrl(
    const GURL& url) const {
  return matcher_->GetHeadersForUrl(url);
}

bool HttpHeaderInjectionService::HasRules() const {
  return matcher_ && !matcher_->IsEmpty();
}

}  // namespace enterprise_custom_headers
