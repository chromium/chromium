// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_ui.h"

#include "ash/constants/webui_url_constants.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/ash/experiences/guest_os/borealis/motd/borealis_motd_page_handler.h"
#include "chromeos/ash/grit/borealis_motd_resources.h"
#include "chromeos/ash/grit/borealis_motd_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_util.h"

namespace {

constexpr const char kBorealisMessageURL[] =
    "https://www.gstatic.com/chromeos-borealis-motd/%d/index.html";

}  // namespace

namespace borealis {

BorealisMOTDUI::BorealisMOTDUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  // Set up the chrome://borealis-motd source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      ash::kChromeUIBorealisMOTDHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, base::span(kBorealisMotdResources),
                              IDR_BOREALIS_MOTD_INDEX_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ObjectSrc, "object-src *;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src *;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-inline';");

  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order).
      {"dismissText", IDS_ASH_BOREALIS_MOTD_DISMISS_TEXT},
      {"placeholderText", IDS_ASH_BOREALIS_MOTD_PLACEHOLDER_TEXT},
      {"titleText", IDS_ASH_BOREALIS_MOTD_TITLE_TEXT},
      {"uninstallText", IDS_ASH_BOREALIS_MOTD_UNINSTALL_TEXT},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddString("motdUrl",
                    base::StringPrintf(kBorealisMessageURL, GetMilestone()));
}

BorealisMOTDUI::~BorealisMOTDUI() = default;

void BorealisMOTDUI::BindInterface(
    mojo::PendingReceiver<ash::borealis_motd::mojom::PageHandlerFactory>
        pending_receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void BorealisMOTDUI::OnPageClosed(UserMotdAction action) {
  base::ListValue args;
  args.Append(GetUserActionString(action));

  // CloseDialog() is a no-op if we are not in a dialog (e.g. user
  // access the page using the URL directly, which is not supported).
  ui::MojoWebDialogUI::CloseDialog(args);
}

}  // namespace borealis
