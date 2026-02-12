// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/proxy_prefs_utils.h"

#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "net/base/proxy_string_util.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace proxy_config {

url::SchemeHostPort ProxyOverrideRuleHostFromString(
    std::string_view raw_value) {
  GURL url(raw_value);
  std::string scheme = url.GetScheme();
  std::string host = url.GetHost();
  int port = url.IntPort();

  // If the value used to initialize `url` is missing parts (for example because
  // it's only a hostname), then try to specify a replacement scheme and/or
  // port. This gives flexibility to the policy field to only specify certain
  // values.
  if (host.empty()) {
    // This path is reached when only the host is provided (ex. "foo.com").
    // In this case, `url` will be invalid and see "foo.com" as a scheme, so if
    // that happens the string is instead parsed as a host:port pair, with HTTP
    // as the default scheme.
    scheme = url::kHttpScheme;
    if (!net::ParseHostAndPort(raw_value, &host, &port)) {
      return url::SchemeHostPort();
    }
  }
  if (scheme.empty()) {
    scheme = url::kHttpScheme;
  }
  if (port == url::PORT_UNSPECIFIED) {
    port = url::DefaultPortForScheme(scheme);
  }

  return url::SchemeHostPort(scheme, host, port);
}

net::ProxyChain ProxyOverrideRuleProxyFromString(std::string_view raw_value) {
  GURL url(raw_value);
  if (url.is_valid()) {
    net::ProxyServer::Scheme scheme = net::GetSchemeFromUriScheme(url.scheme());
    if (scheme == net::ProxyServer::SCHEME_INVALID) {
      return net::ProxyChain();
    }
    return net::ProxyChain::FromSchemeHostAndPort(scheme, url.host(),
                                                  url.port());
  }
  return net::PacResultElementToProxyChain(raw_value);
}

bool ProxyOverrideRulesAllowed(const PrefService* pref_service) {
  CHECK(pref_service);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (pref_service->GetBoolean(prefs::kProxyOverrideRulesAffiliation) ||
      pref_service->GetInteger(prefs::kEnableProxyOverrideRulesForAllUsers) ==
          1) {
    return true;
  }

  const PrefService::Preference* pref =
      pref_service->FindPreference(prefs::kProxyOverrideRules);
  CHECK(pref);

  return !(pref->IsExtensionControlled() ||
           (pref->IsManaged() &&
            pref_service->GetInteger(prefs::kProxyOverrideRulesScope) ==
                policy::POLICY_SCOPE_USER));
#else   // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  return true;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
}

}  // namespace proxy_config
