// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_NETWORK_HEADER_INJECTION_PREFS_H_
#define COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_NETWORK_HEADER_INJECTION_PREFS_H_

class PrefRegistrySimple;

namespace enterprise_custom_headers {
namespace prefs {

// Policy for HTTP header injection.
inline constexpr char kHttpHeaderInjection[] = "http_header_injection";

}  // namespace prefs

// Registers profile preferences related to HTTP header injection.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_custom_headers

#endif  // COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_NETWORK_HEADER_INJECTION_PREFS_H_
