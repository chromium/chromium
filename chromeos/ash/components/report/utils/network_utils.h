// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_NETWORK_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_NETWORK_UTILS_H_

#include <string>

#include "url/gurl.h"

namespace base {
class TimeDelta;
}  // namespace base

class GURL;

namespace network {
struct ResourceRequest;
}  // namespace network

namespace ash::report::utils {

// Production edge server for reporting device actives.
extern const char kFresnelBaseUrl[];

// Fresnel API key.
std::string GetAPIKey();

// Fresnel OPRF forwarding URL.
GURL GetOprfRequestURL();

// Fresnel Query forwarding URL.
GURL GetQueryRequestURL();

// Fresnel Import forwarding URL.
GURL GetImportRequestURL();

// Client side Fresnel OPRF request timeout.
base::TimeDelta GetOprfRequestTimeout();

// Client side Fresnel Query request timeout.
base::TimeDelta GetQueryRequestTimeout();

// Client side Fresnel Import request timeout.
base::TimeDelta GetImportRequestTimeout();

// Max possible fresnel response size.
size_t GetMaxFresnelResponseSizeBytes();

// Creates the resource request with correct headers, url, and HTTP method.
// This is used when sending the network request via. SimpleURLLoader.
std::unique_ptr<network::ResourceRequest> GenerateResourceRequest(
    const GURL& url);

}  // namespace ash::report::utils

#endif  // CHROMEOS_ASH_COMPONENTS_REPORT_UTILS_NETWORK_UTILS_H_
