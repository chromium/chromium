// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gcpw_version.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

GcpwVersion::GcpwVersion() : version_{0, 0, 0, 0} {}

GcpwVersion::GcpwVersion(const std::string& version_str) : GcpwVersion() {
  if (version_str.empty())
    return;

  std::vector<std::string> components = base::SplitString(
      version_str, ".", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
  for (size_t i = 0; i < std::min(version_.size(), components.size()); ++i) {
    unsigned value;
    if (!base::StringToUint(components[i], &value))
      break;

    version_[i] = value;
  }
}

GcpwVersion::GcpwVersion(const GcpwVersion& other) : version_(other.version_) {}

std::string GcpwVersion::ToString() const {
  return base::StringPrintf("%d.%d.%d.%d", version_[0], version_[1],
                            version_[2], version_[3]);
}

unsigned GcpwVersion::major() const {
  return version_[0];
}

unsigned GcpwVersion::minor() const {
  return version_[1];
}

unsigned GcpwVersion::build() const {
  return version_[2];
}

unsigned GcpwVersion::patch() const {
  return version_[3];
}

bool GcpwVersion::operator==(const GcpwVersion& other) const {
  return version_ == other.version_;
}

GcpwVersion& GcpwVersion::operator=(const GcpwVersion& other) {
  version_ = other.version_;
  return *this;
}

bool GcpwVersion::operator<(const GcpwVersion& other) const {
  for (size_t i = 0; i < version_.size(); ++i) {
    if (version_[i] < other.version_[i]) {
      return true;
    } else if (version_[i] > other.version_[i]) {
      return false;
    }
  }
  return false;
}

bool GcpwVersion::IsValid() const {
  return !(*this == GcpwVersion());
}

// static
GcpwVersion GcpwVersion::GetCurrentVersion() {
  base::win::RegKey key;
  LONG status = key.Create(HKEY_LOCAL_MACHINE, kRegUpdaterClientsAppPath,
                           KEY_READ | KEY_WOW64_32KEY);
  if (status == ERROR_SUCCESS) {
    std::wstring version_wstr;
    status = key.ReadValue(kRegVersionName, &version_wstr);
    if (status == ERROR_SUCCESS)
      return GcpwVersion(base::WideToUTF8(version_wstr));
  }

  LOGFN(ERROR) << "Unable to read version from omaha key="
               << kRegUpdaterClientsAppPath << " status=" << status;
  return GcpwVersion();
}
}  // namespace credential_provider
