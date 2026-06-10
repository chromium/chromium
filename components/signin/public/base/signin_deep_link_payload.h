// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_PAYLOAD_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_PAYLOAD_H_

#include <optional>
#include <string>

namespace signin {

// The external device entry point for sign-in flow.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.signin.base
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ExternalEntryPoint
// LINT.IfChange(ExternalEntryPoint)
enum class ExternalEntryPoint : int {
  kUnknown = 0,
  kDesktopDefault = 1,
  kMaxValue = kDesktopDefault,
};
// LINT.ThenChange(
//   //components/signin/public/base/signin_deep_link_parser.cc:ExternalEntryPoint,
//   //tools/metrics/histograms/metadata/signin/histograms.xml:ExternalEntryPoint
// )

// The payload of the signin deep link.
struct SigninDeepLinkPayload {
  // The external device entry point ID value read from the deep link.
  // Set to std::nullopt if the entry point ID query parameter is not present or
  // cannot be parsed. Set to kUnknown if the entry point ID was specified as an
  // integer but with unknown value. Otherwise, set to the value of the entry
  // point ID.
  std::optional<ExternalEntryPoint> entry_point_id = std::nullopt;
  // The raw integer value of the entry point ID read from the deep link.
  // Should only be used for metrics purposes.
  // Set to std::nullopt if the entry point ID query parameter is not present or
  // cannot be parsed as an integer.
  std::optional<int> entry_point_id_raw_value_for_metrics = std::nullopt;

  // The email address value read from the deep link.
  // Set to std::nullopt if the email address query parameter is not present or
  // cannot be parsed.
  std::optional<std::string> email = std::nullopt;

  bool operator==(const SigninDeepLinkPayload&) const = default;

  // Returns true if the payload has all required fields.
  bool HasAllRequiredFields() const;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_DEEP_LINK_PAYLOAD_H_
