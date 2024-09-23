// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_CHROME_XR_INTEGRATION_CLIENT_H_
#define CHROME_BROWSER_VR_CHROME_XR_INTEGRATION_CLIENT_H_

#include <memory>

#include "base/types/pass_key.h"
#include "content/public/browser/browser_xr_runtime.h"
#include "content/public/browser/xr_integration_client.h"

class ChromeContentBrowserClient;

namespace vr {

class ChromeXrIntegrationClient : public content::XrIntegrationClient {
 public:
  explicit ChromeXrIntegrationClient(
      base::PassKey<ChromeContentBrowserClient>) {}
  ~ChromeXrIntegrationClient() override = default;
  ChromeXrIntegrationClient(const ChromeXrIntegrationClient&) = delete;
  ChromeXrIntegrationClient& operator=(const ChromeXrIntegrationClient&) =
      delete;

  // XrIntegrationClient
  std::unique_ptr<content::XrInstallHelper> GetInstallHelper(
      device::mojom::XRDeviceId device_id) override;
  content::XRProviderList GetAdditionalProviders() override;
  std::unique_ptr<content::BrowserXRRuntime::Observer> CreateRuntimeObserver()
      override;

  std::unique_ptr<content::VrUiHost> CreateVrUiHost(
      content::WebContents& contents,
      const std::vector<device::mojom::XRViewPtr>& views,
      mojo::PendingRemote<device::mojom::ImmersiveOverlay> overlay) override;

 private:
  ChromeXrIntegrationClient() = default;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_CHROME_XR_INTEGRATION_CLIENT_H_
