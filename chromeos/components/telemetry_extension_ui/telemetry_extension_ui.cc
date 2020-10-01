// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/telemetry_extension_ui.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chromeos/components/telemetry_extension_ui/diagnostics_service.h"
#include "chromeos/components/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "chromeos/components/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "chromeos/components/telemetry_extension_ui/mojom/system_events_service.mojom.h"
#include "chromeos/components/telemetry_extension_ui/probe_service.h"
#include "chromeos/components/telemetry_extension_ui/system_events_service.h"
#include "chromeos/components/telemetry_extension_ui/telemetry_extension_untrusted_source.h"
#include "chromeos/components/telemetry_extension_ui/url_constants.h"
#include "chromeos/grit/chromeos_telemetry_extension_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace chromeos {

namespace {

std::unique_ptr<content::WebUIDataSource>
CreateTrustedTelemetryExtensionDataSource() {
  auto trusted_source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUITelemetryExtensionHost));

  trusted_source->AddResourcePath("", IDR_TELEMETRY_EXTENSION_INDEX_HTML);
  trusted_source->AddResourcePath("app_icon_96.png",
                                  IDR_TELEMETRY_EXTENSION_ICON_96);
  trusted_source->AddResourcePath("trusted_scripts.js",
                                  IDR_TELEMETRY_EXTENSION_TRUSTED_SCRIPTS_JS);
  trusted_source->AddResourcePath(
      "diagnostics_service.mojom-lite.js",
      IDR_TELEMETRY_EXTENSION_DIAGNOSTICS_SERVICE_MOJO_LITE_JS);
  trusted_source->AddResourcePath(
      "probe_service.mojom-lite.js",
      IDR_TELEMETRY_EXTENSION_PROBE_SERVICE_MOJO_LITE_JS);
  trusted_source->AddResourcePath(
      "system_events_service.mojom-lite.js",
      IDR_TELEMETRY_EXTENSION_SYSTEM_EVENTS_SERVICE_MOJO_LITE_JS);

#if !DCHECK_IS_ON()
  // If a user goes to an invalid url and non-DCHECK mode (DHECK = debug mode)
  // is set, serve a default page so the user sees your default page instead
  // of an unexpected error. But if DCHECK is set, the user will be a
  // developer and be able to identify an error occurred.
  trusted_source->SetDefaultResource(IDR_TELEMETRY_EXTENSION_INDEX_HTML);
#endif  // !DCHECK_IS_ON()

  // We need a CSP override to use the chrome-untrusted:// scheme in the host.
  std::string csp =
      std::string("frame-src ") + kChromeUIUntrustedTelemetryExtensionURL + ";";
  trusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);

  return trusted_source;
}

std::unique_ptr<TelemetryExtensionUntrustedSource>
CreateUntrustedTelemetryExtensionDataSource() {
  auto untrusted_source = TelemetryExtensionUntrustedSource::Create(
      chromeos::kChromeUIUntrustedTelemetryExtensionURL);

  untrusted_source->AddResourcePath("dpsl.js", IDR_TELEMETRY_EXTENSION_DPSL_JS);

  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameAncestors,
      std::string("frame-ancestors ") +
          chromeos::kChromeUITelemetryExtensionURL + ";");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src 'self';");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types telemetry-extension-static;");

  return untrusted_source;
}

}  // namespace

TelemetryExtensionUI::TelemetryExtensionUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();

  content::WebUIDataSource::Add(
      browser_context, CreateTrustedTelemetryExtensionDataSource().release());
  content::URLDataSource::Add(browser_context,
                              CreateUntrustedTelemetryExtensionDataSource());

  // Add ability to request chrome-untrusted: URLs
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
}

TelemetryExtensionUI::~TelemetryExtensionUI() = default;

void TelemetryExtensionUI::BindInterface(
    mojo::PendingReceiver<health::mojom::DiagnosticsService> receiver) {
  diagnostics_service_ =
      std::make_unique<DiagnosticsService>(std::move(receiver));
}

void TelemetryExtensionUI::BindInterface(
    mojo::PendingReceiver<health::mojom::ProbeService> receiver) {
  probe_service_ = std::make_unique<ProbeService>(std::move(receiver));
}

void TelemetryExtensionUI::BindInterface(
    mojo::PendingReceiver<health::mojom::SystemEventsService> receiver) {
  system_events_service_ =
      std::make_unique<SystemEventsService>(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(TelemetryExtensionUI)

}  // namespace chromeos
