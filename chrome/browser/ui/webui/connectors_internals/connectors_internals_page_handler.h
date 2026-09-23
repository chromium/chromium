// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/browser/reporting/report_request.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/connectors_internals.mojom.h"
#include "components/enterprise/connectors/core/connectors_internals_utils.h"
#include "components/enterprise/connectors/core/provisioning_domain_refresh_helper.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace enterprise_reporting {
class ChromeProfileRequestGenerator;
enum class ReportGenerationError;
}  // namespace enterprise_reporting

namespace enterprise_connectors {

// Concrete implementation of connectors_internals::mojom::PageHandler.
class ConnectorsInternalsPageHandler
    : public connectors_internals::mojom::PageHandler {
 public:
  ConnectorsInternalsPageHandler(
      mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver,
      Profile* profile);

  ConnectorsInternalsPageHandler(const ConnectorsInternalsPageHandler&) =
      delete;
  ConnectorsInternalsPageHandler& operator=(
      const ConnectorsInternalsPageHandler&) = delete;

  ~ConnectorsInternalsPageHandler() override;

 private:
  // connectors_internals::mojom::ConnectorsInternalsPageHandler
  void GetDeviceTrustState(GetDeviceTrustStateCallback callback) override;
  void DeleteDeviceTrustKey(DeleteDeviceTrustKeyCallback callback) override;
  void GetClientCertificateState(
      GetClientCertificateStateCallback callback) override;
  void GetSignalsReportingState(
      GetSignalsReportingStateCallback callback) override;
  void GetProvisioningDomainState(
      GetProvisioningDomainStateCallback callback) override;
  void RefreshProvisioningDomainConfigs(
      RefreshProvisioningDomainConfigsCallback callback) override;

  void OnReportGenerated(
      GetSignalsReportingStateCallback callback,
      connectors_internals::mojom::SignalsReportingStatePtr state,
      base::expected<enterprise_reporting::ReportRequestQueue,
                     enterprise_reporting::ReportGenerationError> result);

  void OnSignalsCollected(GetDeviceTrustStateCallback callback,
                          bool is_device_trust_enabled,
                          const base::DictValue signals);

  mojo::Receiver<connectors_internals::mojom::PageHandler> receiver_;
  raw_ptr<Profile> profile_;
  std::unique_ptr<enterprise_reporting::ChromeProfileRequestGenerator>
      request_generator_;

#if BUILDFLAG(ENTERPRISE_PROXY)
  ProvisioningDomainRefreshHelper pvd_refresh_helper_;
#endif

  base::WeakPtrFactory<ConnectorsInternalsPageHandler> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_PAGE_HANDLER_H_
