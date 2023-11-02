// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_GCPW_VERSION_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_GCPW_VERSION_H_

#include <array>
#include <string>

#include "base/component_export.h"

namespace credential_provider {

// A structure to hold the version of GCPW.
class COMPONENT_EXPORT(GCPW_POLICIES) GcpwVersion {
 public:
  // Create a default version which is not valid.
  GcpwVersion();

  // Construct using the given version string specified in
  // "major.minor.build.patch" format.
  GcpwVersion(const std::string& version_str);

  // Copy constructor.
  GcpwVersion(const GcpwVersion& other);

  // Gets the installed version of the GCPW client on this device.
  static GcpwVersion GetCurrentVersion();

  // Returns a formatted string.
  std::string ToString() const;

  // Returns major component of version.
  unsigned major() const;

  // Returns minor component of version.
  unsigned minor() const;

  // Returns build component of version.
  unsigned build() const;

  // Returns patch component of version.
  unsigned patch() const;

  bool operator==(const GcpwVersion& other) const;

  // Assignment operator.
  GcpwVersion& operator=(const GcpwVersion& other);

  // Returns true when this version is less than |other| version.
  bool operator<(const GcpwVersion& other) const;

  // Returns true if this is a valid version.
  bool IsValid() const;

 private:
  std::array<unsigned, 4> version_;
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_GCPW_VERSION_H_
