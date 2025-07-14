// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/browser_management_service.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/new_tab_footer/footer_context_menu.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia_rep_default.h"
#include "ui/gfx/paint_vector_icon.h"

NewTabFooterHandler::NewTabFooterHandler(
    mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>
        pending_handler,
    mojo::PendingRemote<new_tab_footer::mojom::NewTabFooterDocument>
        pending_document,
    base::WeakPtr<TopChromeWebUIController::Embedder> embedder,
    content::WebContents* web_contents)
    : embedder_(embedder),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      document_(std::move(pending_document)),
      handler_{this, std::move(pending_handler)} {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(profile_));
  profile_pref_change_registrar_.Init(profile_->GetPrefs());
  profile_pref_change_registrar_.Add(
      prefs::kNTPFooterExtensionAttributionEnabled,
      base::BindRepeating(&NewTabFooterHandler::UpdateNtpExtensionName,
                          base::Unretained(this)));
  local_state_pref_change_registrar_.Init(g_browser_process->local_state());
  local_state_pref_change_registrar_.Add(
      prefs::kNTPFooterManagementNoticeEnabled,
      base::BindRepeating(&NewTabFooterHandler::UpdateManagementNotice,
                          base::Unretained(this)));
  local_state_pref_change_registrar_.Add(
      prefs::kEnterpriseCustomLabelForBrowser,
      base::BindRepeating(&NewTabFooterHandler::UpdateManagementNotice,
                          base::Unretained(this)));
}

NewTabFooterHandler::~NewTabFooterHandler() = default;

void NewTabFooterHandler::UpdateNtpExtensionName() {
  std::string id;
  std::string name;
  bool attribution_enabled = profile_->GetPrefs()->GetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled);
  if (attribution_enabled) {
    if (const extensions::Extension* ntp_extension =
            extensions::GetExtensionOverridingNewTabPage(profile_)) {
      id = ntp_extension->id();
      name = ntp_extension->name();
    }
  }
  curr_ntp_extension_id_ = id;
  document_->SetNtpExtensionName(std::move(name));
  }

void NewTabFooterHandler::OpenExtensionOptionsPageWithFallback() {
  GURL options_url = GURL(chrome::kChromeUIExtensionsURL);
  if (!curr_ntp_extension_id_.empty()) {
    options_url = net::AppendOrReplaceQueryParameter(options_url, "id",
                                                     curr_ntp_extension_id_);
  }
  OpenUrlInCurrentTab(options_url);
}

void NewTabFooterHandler::ShowContextMenu(const gfx::Point& point) {
  const bool is_managed =
      enterprise_util::CanShowEnterpriseBadgingForNTPFooter(profile_);
  // TODO(crbug.com/424878134): Add managed-specific behavior.
  if (embedder_ && !is_managed) {
    embedder_->ShowContextMenu(point,
                               std::make_unique<FooterContextMenu>(profile_));
  }
}

void NewTabFooterHandler::UpdateManagementNotice() {
  if (!enterprise_util::CanShowEnterpriseBadgingForNTPFooter(profile_)) {
    document_->SetManagementNotice(nullptr);
    return;
  }

  auto notice = new_tab_footer::mojom::ManagementNotice::New();
  notice->text = GetManagementNoticeText();
  notice->bitmap_data_url =
      GURL(webui::GetBitmapDataUrl(GetManagementNoticeIconBitmap()));
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

SkBitmap NewTabFooterHandler::GetManagementNoticeIconBitmap() {
  CHECK(enterprise_util::CanShowEnterpriseBadgingForNTPFooter(profile_));

  // Return custom icon if set by policy.
  gfx::Image* custom_icon =
      policy::ManagementServiceFactory::GetForProfile(profile_)
          ->GetManagementIconForBrowser();
  if (custom_icon && !custom_icon->IsEmpty()) {
    return custom_icon->AsBitmap();
  }

  const gfx::ImageSkia default_management_icon =
      gfx::CreateVectorIcon(gfx::IconDescription(
          vector_icons::kBusinessIcon, 20,
          web_contents_->GetColorProvider().GetColor(ui::kColorIcon)));
  return default_management_icon.GetRepresentation(1.0f).GetBitmap();
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
