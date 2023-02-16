// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/vm/vm_ui.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_diagnostics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/vm/vm.mojom.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {

namespace {

class VmUIImpl : public vm::mojom::VmDiagnosticsProvider {
 public:
  explicit VmUIImpl(
      mojo::PendingReceiver<vm::mojom::VmDiagnosticsProvider> receiver)
      : receiver_(this, std::move(receiver)) {}

 private:
  // vm::mojom::VmDiagnosticsProvider:
  void GetPluginVmDiagnostics(
      GetPluginVmDiagnosticsCallback callback) override {
    plugin_vm::GetDiagnostics(std::move(callback));
  }

  mojo::Receiver<vm::mojom::VmDiagnosticsProvider> receiver_;
};

void AddStringResources(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"contentsPageTitle", IDS_VM_STATUS_INDEX_PAGE_TITLE},
      {"passLabel", IDS_VM_STATUS_PAGE_PASS_LABEL},
      {"failLabel", IDS_VM_STATUS_PAGE_FAIL_LABEL},
      {"notApplicableLabel", IDS_VM_STATUS_PAGE_NOT_APPLICABLE_LABEL},
      {"notEnabledMessage", IDS_VM_STATUS_PAGE_NOT_ENABLED_MESSAGE},
      {"learnMoreLabel", IDS_LEARN_MORE},
      {"requirementLabel", IDS_VM_STATUS_PAGE_REQUIREMENT_LABEL},
      {"statusLabel", IDS_VM_STATUS_PAGE_STATUS_LABEL},
      {"explanationLabel", IDS_VM_STATUS_PAGE_EXPLANATION_LABEL},
      {"pageTitle", IDS_VM_STATUS_PAGE_TITLE},
      {"pluginVmAppName", IDS_PLUGIN_VM_APP_NAME},
  };
  source->AddLocalizedStrings(kStrings);
}

}  // namespace

VmUI::VmUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  auto* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(profile, chrome::kChromeUIVmHost);
  webui::SetJSModuleDefaults(source);

  AddStringResources(source);

  source->SetDefaultResource(IDR_VM_INDEX_HTML);
  source->AddResourcePath("app.js", IDR_VM_APP_JS);
  source->AddResourcePath("vm.mojom-webui.js", IDR_VM_MOJOM_WEBUI_JS);
  source->AddResourcePath("guest_os_diagnostics.mojom-webui.js",
                          IDR_GUEST_OS_DIAGNOSTICS_MOJOM_WEBUI_JS);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types "
      // Add TrustedTypes policies necessary for using Polymer.
      "polymer-html-literal polymer-template-event-attribute-policy;");
}

VmUI::~VmUI() = default;

void VmUI::BindInterface(
    mojo::PendingReceiver<vm::mojom::VmDiagnosticsProvider> receiver) {
  ui_handler_ = std::make_unique<VmUIImpl>(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(VmUI)

}  // namespace ash
