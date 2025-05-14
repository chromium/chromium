// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition.h"

NewTabFooterHandler::NewTabFooterHandler(
    mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>
        pending_handler,
    mojo::PendingRemote<new_tab_footer::mojom::NewTabFooterDocument>
        pending_document,
    content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      document_(std::move(pending_document)),
      handler_{this, std::move(pending_handler)} {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(profile_));
}

NewTabFooterHandler::~NewTabFooterHandler() = default;

void NewTabFooterHandler::UpdateNtpExtensionName() {
  const extensions::Extension* ntp_extension =
      extensions::GetExtensionOverridingNewTabPage(profile_);
  if (!ntp_extension) {
    curr_ntp_extension_id_ = std::string();
    document_->SetNtpExtensionName(std::string());
    return;
  }

  curr_ntp_extension_id_ = ntp_extension->id();
  document_->SetNtpExtensionName(std::move(ntp_extension->name()));
}

void NewTabFooterHandler::OpenExtensionOptionsPageWithFallback() {
  GURL options_url = GURL(chrome::kChromeUIExtensionsURL);
  if (!curr_ntp_extension_id_.empty()) {
    options_url = net::AppendOrReplaceQueryParameter(options_url, "id",
                                                     curr_ntp_extension_id_);
  }
  OpenUrlInCurrentTab(options_url);
}

void NewTabFooterHandler::UpdateManagementNotice() {
  if (!enterprise_util::CanShowEnterpriseBadgingForNTPFooter(profile_)) {
    document_->SetManagementNotice(nullptr);
    return;
  }

  auto notice = new_tab_footer::mojom::ManagementNotice::New();
  notice->text = GetManagementNoticeText();
  document_->SetManagementNotice(std::move(notice));
}

void NewTabFooterHandler::OpenUrlInCurrentTab(const GURL& url) {
  auto* browser_window = webui::GetBrowserWindowInterface(web_contents_);
  if (!browser_window || !url.is_valid()) {
    return;
  }

  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  browser_window->OpenURL(params, /*navigation_handle_callback=*/{});
}

std::string NewTabFooterHandler::GetManagementNoticeText() {
  CHECK(enterprise_util::CanShowEnterpriseBadgingForNTPFooter(profile_));

  // Return "Managed by <label>" if custom label is set.
  std::string custom_label = g_browser_process->local_state()->GetString(
      prefs::kEnterpriseCustomLabelForBrowser);
  if (!custom_label.empty()) {
    return l10n_util::GetStringFUTF8(IDS_MANAGED_BY,
                                     base::UTF8ToUTF16(custom_label));
  }

  // Return "Managed by <management domain>" if a cloud manager is known.
  // Otherwise return the generic "Managed by your organization" message.
  std::optional<std::string> cloud_policy_manager = GetDeviceManagerIdentity();
  return cloud_policy_manager && !cloud_policy_manager->empty()
             ? l10n_util::GetStringFUTF8(
                   IDS_MANAGED_BY, base::UTF8ToUTF16(*cloud_policy_manager))
             : l10n_util::GetStringUTF8(IDS_MANAGED);
}

void NewTabFooterHandler::OnExtensionReady(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  UpdateNtpExtensionName();
}

void NewTabFooterHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  UpdateNtpExtensionName();
}
