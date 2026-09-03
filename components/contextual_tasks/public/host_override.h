// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_HOST_OVERRIDE_H_
#define COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_HOST_OVERRIDE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

class GURL;

namespace contextual_tasks {

// Encapsulates a host override used to route embedded page requests in
// Contextual Tasks (e.g., to redirect traffic to local development servers or
// staging environments). Provides utilities to parse and format host override
// representations (including IPv6 literals), check whether a URL matches the
// override, and apply the override to a GURL.
struct HostOverride {
  std::string host;
  std::optional<uint16_t> port = std::nullopt;

  // Deserializer: parses "<host>[:<port>]" from string inputs.
  static std::optional<HostOverride> FromString(std::string_view str);

  // Serializer: formats as "host" or "host:port" for string outputs.
  std::string ToString() const;

  // Matcher: checks if url's host and port match this override.
  bool Matches(const GURL& url) const;

  // Mutator: safely updates url's host and port via GURL::Replacements.
  GURL ApplyToUrl(const GURL& url) const;

  friend bool operator==(const HostOverride&, const HostOverride&) = default;
};

}  // namespace contextual_tasks

#endif  // COMPONENTS_CONTEXTUAL_TASKS_PUBLIC_HOST_OVERRIDE_H_
