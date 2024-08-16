// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/sandbox/sandbox_internals_ui.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/sandbox_internals_resources.h"
#include "chrome/grit/sandbox_internals_resources_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/webui/sandbox/sandbox_handler.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/common/sandbox_status_extension_android.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "content/public/browser/zygote_host/zygote_host_linux.h"
#include "sandbox/policy/sandbox.h"
#endif

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
static void SetSandboxStatusData(content::WebUIDataSource* source) {
  // Get expected sandboxing status of renderers.
  const int status =
      content::ZygoteHost::GetInstance()->GetRendererSandboxStatus();

  source->AddBoolean("suid", status & sandbox::policy::SandboxLinux::kSUID);
  source->AddBoolean("userNs", status & sandbox::policy::SandboxLinux::kUserNS);
  source->AddBoolean("pidNs", status & sandbox::policy::SandboxLinux::kPIDNS);
  source->AddBoolean("netNs", status & sandbox::policy::SandboxLinux::kNetNS);
  source->AddBoolean("seccompBpf",
                     status & sandbox::policy::SandboxLinux::kSeccompBPF);
  source->AddBoolean("seccompTsync",
                     status & sandbox::policy::SandboxLinux::kSeccompTSYNC);
  source->AddBoolean("yamaBroker",
                     status & sandbox::policy::SandboxLinux::kYama);

  // Yama does not enforce in user namespaces.
  bool enforcing_yama_nonbroker =
      status & sandbox::policy::SandboxLinux::kYama &&
      !(status & sandbox::policy::SandboxLinux::kUserNS);
  source->AddBoolean("yamaNonbroker", enforcing_yama_nonbroker);

  // Require either the setuid or namespace sandbox for our first-layer sandbox.
  bool good_layer1 = (status & sandbox::policy::SandboxLinux::kSUID ||
                      status & sandbox::policy::SandboxLinux::kUserNS) &&
                     status & sandbox::policy::SandboxLinux::kPIDNS &&
                     status & sandbox::policy::SandboxLinux::kNetNS;
  // A second-layer sandbox is also required to be adequately sandboxed.
  bool good_layer2 = status & sandbox::policy::SandboxLinux::kSeccompBPF;
  source->AddBoolean("sandboxGood", good_layer1 && good_layer2);
}
#endif

void CreateAndAddDataSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUISandboxHost);
  source->AddResourcePaths(base::make_span(kSandboxInternalsResources,
                                           kSandboxInternalsResourcesSize));
  source->SetDefaultResource(IDR_SANDBOX_INTERNALS_SANDBOX_INTERNALS_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  SetSandboxStatusData(source);
  source->UseStringsJs();
#endif
}

}  // namespace

SandboxInternalsUI::SandboxInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
#if BUILDFLAG(IS_WIN)
  web_ui->AddMessageHandler(
      std::make_unique<sandbox_handler::SandboxHandler>());
#endif
  CreateAndAddDataSource(Profile::FromWebUI(web_ui));
}

void SandboxInternalsUI::WebUIRenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
#if BUILDFLAG(IS_ANDROID)
  mojo::AssociatedRemote<chrome::mojom::SandboxStatusExtension> sandbox_status;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &sandbox_status);
  sandbox_status->AddSandboxStatusExtension();
#endif
}

SandboxInternalsUI::~SandboxInternalsUI() = default;
