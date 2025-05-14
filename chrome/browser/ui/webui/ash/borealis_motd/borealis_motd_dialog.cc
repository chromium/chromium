// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/webui/ash/borealis_motd/borealis_motd_dialog.h"

#include "base/strings/stringprintf.h"
#include "base/version.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/borealis_motd_resources.h"
#include "chrome/grit/borealis_motd_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/webui_util.h"

namespace borealis {

namespace {

constexpr const char kClientActionDismiss[] = "dismiss";
constexpr const char kBorealisMessageURL[] =
    "https://www.gstatic.com/chromeos-borealis-motd/%d/index.html";

int GetMilestone() {
  return version_info::GetVersion().components()[0];
}

}  // namespace

void MaybeShowBorealisMOTDDialog(base::OnceCallback<void()> cb,
                                 content::BrowserContext* context) {
  if (!base::FeatureList::IsEnabled(features::kBorealis)) {
    std::move(cb).Run();
    return;
  }
  return BorealisMOTDDialog::Show(std::move(cb), context);
}

BorealisMOTDUI::BorealisMOTDUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://borealis-motd source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIBorealisMOTDHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, base::span(kBorealisMotdResources),
                              IDR_BOREALIS_MOTD_INDEX_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ObjectSrc, "object-src *;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src *;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' 'unsafe-inline';");

  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order).
      {"dismissText", IDS_BOREALIS_MOTD_DISMISS_TEXT},
      {"placeholderText", IDS_BOREALIS_MOTD_PLACEHOLDER_TEXT},
      {"titleText", IDS_BOREALIS_MOTD_TITLE_TEXT},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddString("motdUrl",
                    base::StringPrintf(kBorealisMessageURL, GetMilestone()));
}

BorealisMOTDUI::~BorealisMOTDUI() = default;

BorealisMOTDDialog::BorealisMOTDDialog(base::OnceCallback<void()> cb)
    : close_callback_(std::move(cb)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  set_allow_default_context_menu(false);
  set_can_close(true);
  set_can_resize(false);
  set_dialog_content_url(GURL(chrome::kChromeUIBorealisMOTDURL));
  set_dialog_frame_kind(ui::WebDialogDelegate::FrameKind::kDialog);
  set_dialog_modal_type(ui::mojom::ModalType::kSystem);
  set_dialog_size(
      gfx::Size(views::LayoutProvider::Get()->GetDistanceMetric(
                    views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH),
                views::LayoutProvider::Get()->GetDistanceMetric(
                    views::DISTANCE_MODAL_DIALOG_SCROLLABLE_AREA_MAX_HEIGHT)));
  set_show_close_button(true);
  set_show_dialog_title(false);
}

// TODO(crbug.com/410835744): Move from //chrome.
void BorealisMOTDDialog::Show(base::OnceCallback<void()> cb,
                              content::BrowserContext* context) {
  // BorealisMOTDDialog is self-deleting via OnDialogClosed().
  chrome::ShowWebDialog(nullptr /* parent */, context,
                        new BorealisMOTDDialog(std::move(cb)));
}
void BorealisMOTDDialog::OnDialogClosed(const std::string& json_retval) {
  std::move(close_callback_).Run();
  delete this;
}

void BorealisMOTDDialog::OnLoadingStateChanged(content::WebContents* source) {
  if (source->GetURL().ref() == kClientActionDismiss) {
    source->ClosePage();
  }
}

BorealisMOTDDialog::~BorealisMOTDDialog() = default;

}  // namespace borealis
