// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_SIGNED_WEB_BUNDLE_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_SIGNED_WEB_BUNDLE_UTILS_H_

#include <memory>

#include "base/callback_forward.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"

namespace web_app {

class SignedWebBundleReader;

// Given the `reader`, read the body of `response` from it and return the result
// as a string.
std::string ReadAndFulfillResponseBody(
    SignedWebBundleReader& reader,
    web_package::mojom::BundleResponsePtr response);

std::string ReadAndFulfillResponseBody(
    uint32_t response_length,
    base::OnceCallback<void(mojo::ScopedDataPipeProducerHandle producer_handle,
                            base::OnceCallback<void(net::Error net_error)>)>
        read_response_body_callback);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_SIGNED_WEB_BUNDLE_UTILS_H_
