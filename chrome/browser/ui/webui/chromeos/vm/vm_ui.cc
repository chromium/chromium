// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/vm/vm_ui.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_diagnostics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/vm/vm.mojom.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/receiver.h"

// TODO(b/173653141) We need to port this to lacros eventually.

namespace chromeos {

namespace {

class VmUIImpl : public vm::mojom::VmDiagnosticsProvider {
 public:
  explicit VmUIImpl(
      mojo::PendingReceiver<vm::mojom::VmDiagnosticsProvider> receiver)
      : receiver_(this, std::move(receiver)) {}

 private:
  // chromeos::vm::mojom::VmDiagnosticsProvider:
  void GetPluginVmDiagnostics(
      GetPluginVmDiagnosticsCallback callback) override {
    plugin_vm::GetDiagnostics(std::move(callback));
  }

  mojo::Receiver<vm::mojom::VmDiagnosticsProvider> receiver_;
};

void AddStringResources(content::WebUIDataSource* source) {
  source->AddString("contentsPageTitle", "VM debugging page");
  source->AddString("pluginVmTitle", "Parallels Desktop status");
  source->AddString("passLabel", "Pass");
  source->AddString("failLabel", "Fail");
  source->AddString("notApplicableLabel", "N/A");
  source->AddString("notEnabledLabel", "Not Enabled");
  source->AddString("learnMoreLabel", "Learn more");

  source->AddString("requirementLabel", "Requirement");
  source->AddString("statusLabel", "Status");
  source->AddString("explanationLabel", "Explanation");
}

}  // namespace

VmUI::VmUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  auto* profile = Profile::FromWebUI(web_ui);
  auto source = base::WrapUnique(
      content::WebUIDataSource::Create(chrome::kChromeUIVmHost));
  webui::SetJSModuleDefaults(source.get());

  AddStringResources(source.get());

  source->SetDefaultResource(IDR_VM_INDEX_HTML);
  source->AddResourcePath("app.js", IDR_VM_APP_JS);
  source->AddResourcePath("vm.mojom-webui.js", IDR_VM_MOJOM_WEBUI_JS);
  source->AddResourcePath("guest_os_diagnostics.mojom-webui.js",
                          IDR_GUEST_OS_DIAGNOSTICS_MOJOM_WEBUI_JS);

  content::WebUIDataSource::Add(profile, source.release());
}

VmUI::~VmUI() = default;

void VmUI::BindInterface(
    mojo::PendingReceiver<vm::mojom::VmDiagnosticsProvider> receiver) {
  ui_handler_ = std::make_unique<VmUIImpl>(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(VmUI)

}  // namespace chromeos
