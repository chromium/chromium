// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_FACILITATED_PAYMENTS_API_CLIENT_FACTORY_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_FACILITATED_PAYMENTS_API_CLIENT_FACTORY_H_

#include <memory>

namespace content {
class RenderFrameHost;
}  // namespace content

namespace payments::facilitated {

class FacilitatedPaymentsApiClient;

// Creates a platform-specific instance of the API client. This function is
// defined in platform specific implementation source files, e.g., in
// `facilitated_payments_api_client_android.cc`.
std::unique_ptr<FacilitatedPaymentsApiClient>
CreateFacilitatedPaymentsApiClient(content::RenderFrameHost* render_frame_host);

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_FACILITATED_PAYMENTS_API_CLIENT_FACTORY_H_
