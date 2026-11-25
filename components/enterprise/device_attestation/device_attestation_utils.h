// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_UTILS_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_UTILS_H_

#include <string>

namespace enterprise_management {
class ChromeProfileReportRequest;
}

namespace enterprise {

// Generates a JSON serialized representation of content binding signals from
// the provided profile report. Returns an empty string if no relevant fields
// are populated (though typically it will return at least {"version": 1}).
//
// DISCLAIMER: Do not update this content binding method without matching the
// changes on the server and incrementing the version number. Any changes should
// be done behind a flag and reflected on the server.
std::string GenerateV1ContentBindingString(
    const enterprise_management::ChromeProfileReportRequest& report);

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_UTILS_H_
