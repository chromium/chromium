// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_internal_authenticator.h"

namespace autofill {

void TestInternalAuthenticator::IsUserVerifyingPlatformAuthenticatorAvailable(
    blink::mojom::Authenticator::
        IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) {
  std::move(callback).Run(false);
}

content::RenderFrameHost* TestInternalAuthenticator::GetRenderFrameHost() {
  return nullptr;
}

}  // namespace autofill
