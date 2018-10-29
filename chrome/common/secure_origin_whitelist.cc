// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/secure_origin_whitelist.h"

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "extensions/common/constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

namespace {

// Given a hostname pattern with a wildcard such as "*.foo.com", returns
// true if |hostname_pattern| meets both of these conditions:
// 1.) A string matching |hostname_pattern| is a valid hostname.
// 2.) Wildcards only appear beyond the eTLD+1. "*.foo.com" is considered
//     valid but "*.com" is not.
bool IsValidWildcardPattern(const std::string& hostname_pattern) {
  // Replace wildcards with dummy values to check whether a matching origin is
  // valid.
  std::string wildcards_replaced;
  if (!base::ReplaceChars(hostname_pattern, "*", "a", &wildcards_replaced))
    return false;
  // Construct a SchemeHostPort with a dummy scheme and port to check that the
  // hostname is valid.
  url::SchemeHostPort scheme_host_port(
      GURL(base::StringPrintf("http://%s:80", wildcards_replaced.c_str())));
  if (scheme_host_port.IsInvalid())
    return false;

  // Check that wildcards only appear beyond the eTLD+1.
  size_t registry_length =
      net::registry_controlled_domains::PermissiveGetHostRegistryLength(
          hostname_pattern,
          net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // std::string::npos should only be returned for empty inputs, which should be
  // filtered out by the IsInvalid() check above.
  CHECK(registry_length != std::string::npos);
  // If there is no registrar portion, the pattern is considered invalid.
  if (registry_length == 0)
    return false;
  // If there is no component before the registrar portion, or if the component
  // immediately preceding the registrar portion contains a wildcard, the
  // pattern is not considered valid.
  std::string host_before_registrar =
      hostname_pattern.substr(0, hostname_pattern.size() - registry_length);
  std::vector<std::string> components =
      base::SplitString(host_before_registrar, ".", base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (components.size() == 0)
    return false;
  if (components.back().find("*") != std::string::npos)
    return false;
  return true;
}

// Canonicalizes each component of |hostname_pattern|, making no changes to
// wildcard components or components that fail canonicalization. For example,
// given a |hostname_pattern| of "TeSt.*.%46oo.com", the output will be
// "test.*.foo.com".
std::string CanonicalizePatternComponents(const std::string& hostname_pattern) {
  std::string canonical_host;  // Do not modify outside of canon_output.
  canonical_host.reserve(hostname_pattern.length());
  url::StdStringCanonOutput canon_output(&canonical_host);

  for (size_t current = 0; current < hostname_pattern.length(); current++) {
    size_t begin = current;

    // Advance to next "." or end.
    current = hostname_pattern.find('.', begin);
    if (current == std::string::npos)
      current = hostname_pattern.length();

    // Try to append the canonicalized version of this component.
    int current_len = base::checked_cast<int>(current - begin);
    if (hostname_pattern.substr(begin, current_len) == "*" ||
        !url::CanonicalizeHostSubstring(
            hostname_pattern.data(),
            url::Component(base::checked_cast<int>(begin), current_len),
            &canon_output)) {
      // Failed to canonicalize this component; append as-is.
      canon_output.Append(hostname_pattern.substr(begin, current_len).data(),
                          current_len);
    }

    if (current < hostname_pattern.length())
      canon_output.push_back('.');
  }
  canon_output.Complete();
  return canonical_host;
}

}  // namespace

namespace secure_origin_whitelist {

std::vector<std::string> ParseWhitelist(const std::string& origins_str) {
  std::vector<std::string> origin_patterns;
  for (const std::string& origin_str : base::SplitString(
           origins_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (origin_str.find("*") != std::string::npos) {
      if (IsValidWildcardPattern(origin_str)) {
        std::string canonicalized_pattern =
            CanonicalizePatternComponents(origin_str);
        if (!canonicalized_pattern.empty()) {
          origin_patterns.push_back(canonicalized_pattern);
          continue;
        }
      }
      LOG(ERROR) << "Whitelisted secure origin pattern " << origin_str
                 << " is not valid; ignoring.";
      continue;
    }

    // Drop .unique() origins, as they are unequal to any other origins.
    url::Origin origin(url::Origin::Create(GURL(origin_str)));
    if (!origin.opaque())
      origin_patterns.push_back(origin.Serialize());
  }

  UMA_HISTOGRAM_COUNTS_100("Security.TreatInsecureOriginAsSecure",
                           origin_patterns.size());

#if defined(OS_CHROMEOS)
  // For Crostini, we allow access to the default VM/container as a secure
  // origin via the hostname penguin.linux.test. We are required to use a
  // wildcard for the prefix because we do not know what the port number is.
  // https://chromium.googlesource.com/chromiumos/docs/+/master/containers_and_vms.md
  origin_patterns.push_back("*.linux.test");
#endif

  return origin_patterns;
}

std::vector<std::string> GetWhitelist() {
  // If kUnsafelyTreatInsecureOriginAsSecure option is given, then treat the
  // value as a comma-separated list of origins or origin patterns. Callers that
  // need to also check the kUnsafelyTreatInsecureOriginAsSecure pref value must
  // instead use ParseWhitelist directly (as there is no way for GetWhitelist()
  // to access prefs). For renderer processes the pref and the switch will
  // match, but for non-renderer processes the switch may not be set.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string origins_str = "";
  if (command_line.HasSwitch(switches::kUnsafelyTreatInsecureOriginAsSecure)) {
    origins_str = command_line.GetSwitchValueASCII(
        switches::kUnsafelyTreatInsecureOriginAsSecure);
  }
  return ParseWhitelist(origins_str);
}

std::set<std::string> GetSchemesBypassingSecureContextCheck() {
  std::set<std::string> schemes;
  schemes.insert(extensions::kExtensionScheme);
  return schemes;
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kUnsafelyTreatInsecureOriginAsSecure,
                               /* default_value */ "");
}

}  // namespace secure_origin_whitelist
