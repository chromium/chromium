// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/certificate_manager_localized_strings_provider.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
#include "chrome/browser/ui/webui/certificate_manager/client_cert_sources.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/certificate_provisioning_ui_handler.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

const char kCRSLearnMoreLink[] =
    "https://chromium.googlesource.com/chromium/src/+/main/net/data/ssl/"
    "chrome_root_store/faq.md";

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AddCertificateManagerStrings(content::WebUIDataSource* html_source) {
  struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"cancel", IDS_CANCEL},
      {"close", IDS_CLOSE},
      {"edit", IDS_SETTINGS_EDIT},
      {"moreActions", IDS_SETTINGS_MORE_ACTIONS},
      {"ok", IDS_OK},
  };
  for (const auto& entry : localized_strings) {
    html_source->AddLocalizedString(entry.name, entry.id);
  }
  certificate_manager::AddLocalizedStrings(html_source);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
void AddCertificateManagerV2Strings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"ok", IDS_OK},
      {"cancel", IDS_CANCEL},
      {"opensInNewTab", IDS_SETTINGS_OPENS_IN_NEW_TAB},
      {"certificateManagerV2Title", IDS_SETTINGS_CERTIFICATE_MANAGER_V2_TITLE},
      {"certificateManagerV2ClientCerts",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CLIENT_CERTIFICATES},
      {"certificateManagerV2ClientCertsDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CLIENT_CERTIFICATES_DESCRIPTION},
      {"certificateManagerV2ClientCertsFromPlatform",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CLIENT_CERTIFICATES_FROM_PLATFORM},
      {"certificateManagerV2ClientCertsFromExtension",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CLIENT_CERTIFICATES_FROM_EXTENSION},
      {"certificateManagerV2ClientCertsFromAdmin",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CLIENT_CERTIFICATES_FROM_ADMIN},
      {"certificateManagerV2LocalCerts",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_LOCAL_CERTIFICATES},
      {"certificateManagerV2LocalCertsDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_LOCAL_CERTIFICATES_DESCRIPTION},
      {"certificateManagerV2CRSCerts",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CRS_CERTIFICATES},
      {"certificateManagerV2CRSCertsDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CRS_CERTIFICATES_DESCRIPTION},
      {"certificateManagerV2CRSLearnMoreLink",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CRS_LEARN_MORE_LINK},
      {"certificateManagerV2CRSLearnMoreLinkAriaLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CRS_LEARN_MORE_LINK_ARIA_LABEL},
      {"certificateManagerV2HashCopiedToast",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_HASH_COPIED_TOAST},
      {"certificateManagerV2AdminCertsTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_ADMIN_CERTS_TITLE},
      {"certificateManagerV2CustomCertsTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_CUSTOM_CERTS_TITLE},
      {"certificateManagerV2TrustedCertsList",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_TRUSTED_CERTS_LIST},
      {"certificateManagerV2IntermediateCertsList",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_INTERMEDIATE_CERTS_LIST},
      {"certificateManagerV2DistrustedCertsList",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DISTRUSTED_CERTS_LIST},
      {"certificateManagerV2NoCertificatesRow",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_NO_CERTIFICATES_ROW},
      {"certificateManagerV2ExportButtonLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_EXPORT_BUTTON_LABEL},
      {"certificateManagerV2ExportButtonAriaLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_EXPORT_BUTTON_ARIA_LABEL},
      {"certificateManagerV2DeleteErrorTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_DELETE_ERROR_TITLE},
      {"certificateManagerV2ImportErrorTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_ERROR_TITLE},
      {"certificateManagerV2ImportButtonLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_BUTTON_LABEL},
      {"certificateManagerV2ImportButtonAriaLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_BUTTON_ARIA_LABEL},
      {"certificateManagerV2ImportAndBindButtonLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_AND_BIND_BUTTON_LABEL},
      {"certificateManagerV2ImportAndBindButtonAriaLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_IMPORT_AND_BIND_BUTTON_ARIA_LABEL},
      {"certificateManagerV2EnterPasswordTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_ENTER_PASSWORD_TITLE},
      {"certificateManagerV2PlatformCertsTitle",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_PLATFORM_CERTS_TITLE},
      {"certificateManagerV2PlatformCertsToggleLabel",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_PLATFORM_CERTS_TOGGLE_LABEL},
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      {"certificateManagerV2PlatformCertsManageLink",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_PLATFORM_CERTS_MANAGE_LINK},
      {"certificateManagerV2PlatformCertsManageLinkAriaDescription",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_PLATFORM_CERTS_MANAGE_LINK_ARIA_DESCRIPTION},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      {"certificateManagerV2PlatformCertsViewLink",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_PLATFORM_CERTS_VIEW_LINK},
      {"certificateManagerV2Platform",
       IDS_SETTINGS_CERTIFICATE_MANAGER_V2_PLATFORM},
      {"certificateManagerV2SubpageBackButtonAriaLabel",
       IDS_CERTIFICATE_MANAGER_V2_SUBPAGE_BACK_BUTTON_ARIA_LABEL},
      {"certificateManagerV2SubpageBackButtonAriaRoleDescription",
       IDS_CERTIFICATE_MANAGER_V2_SUBPAGE_BACK_BUTTON_ARIA_ROLE_DESCRIPTION},
      {"certificateManagerV2CertEntryViewAriaLabel",
       IDS_CERTIFICATE_MANAGER_V2_CERT_ENTRY_VIEW_ARIA_LABEL},
      {"certificateManagerV2CertEntryDeleteAriaLabel",
       IDS_CERTIFICATE_MANAGER_V2_CERT_ENTRY_DELETE_ARIA_LABEL},
      {"certificateManagerV2CertHashCopyAriaLabel",
       IDS_CERTIFICATE_MANAGER_V2_CERT_HASH_COPY_ARIA_LABEL},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

}  // namespace

CertificateManagerUI::CertificateManagerUI(content::WebUI* web_ui)
#if BUILDFLAG(IS_CHROMEOS)
    : MojoWebDialogUI(web_ui) {
#else
    : MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
#endif
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUICertificateManagerHost);
  webui::EnableTrustedTypesCSP(source);
  webui::SetJSModuleDefaults(source);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(features::kEnableCertManagementUIV2)) {
    // Serve the old certificate manager
    AddCertificateManagerStrings(source);

    source->UseStringsJs();
    source->SetDefaultResource(IDR_CERT_MANAGER_DIALOG_HTML);

    source->AddBoolean("isGuest",
                       user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
                           user_manager::UserManager::Get()
                               ->IsLoggedInAsManagedGuestSession());
    source->AddBoolean(
        "isKiosk", user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp());

    web_ui->AddMessageHandler(
        std::make_unique<certificate_manager::CertificatesHandler>());
    web_ui->AddMessageHandler(
        chromeos::cert_provisioning::CertificateProvisioningUiHandler::
            CreateForProfile(profile));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kEnableCertManagementUIV2)) {
    source->AddResourcePath("", IDR_CERT_MANAGER_DIALOG_V2_HTML);
    AddCertificateManagerV2Strings(source);
    source->AddString("crsLearnMoreUrl", kCRSLearnMoreLink);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ClientCertManagementAccessControls client_cert_policy(profile);
    source->AddBoolean(
        "clientCertImportAllowed",
        client_cert_policy.IsManagementAllowed(
            ClientCertManagementAccessControls::kSoftwareBacked));
    source->AddBoolean(
        "clientCertImportAndBindAllowed",
        client_cert_policy.IsManagementAllowed(
            ClientCertManagementAccessControls::kHardwareBacked));
#endif

    auto plural_string_handler = std::make_unique<PluralStringHandler>();
    plural_string_handler->AddLocalizedString(
        "certificateManagerV2NumCerts",
        IDS_SETTINGS_CERTIFICATE_MANAGER_V2_NUM_CERTS);
    web_ui->AddMessageHandler(std::move(plural_string_handler));
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
void CertificateManagerUI::BindInterface(
    mojo::PendingReceiver<
        certificate_manager_v2::mojom::CertificateManagerPageHandlerFactory>
        pending_receiver) {
  if (certificate_manager_handler_factory_receiver_.is_bound()) {
    certificate_manager_handler_factory_receiver_.reset();
  }
  certificate_manager_handler_factory_receiver_.Bind(
      std::move(pending_receiver));
}

void CertificateManagerUI::CreateCertificateManagerPageHandler(
    mojo::PendingRemote<certificate_manager_v2::mojom::CertificateManagerPage>
        client,
    mojo::PendingReceiver<
        certificate_manager_v2::mojom::CertificateManagerPageHandler> handler) {
  certificate_manager_page_handler_ =
      std::make_unique<CertificateManagerPageHandler>(
          std::move(client), std::move(handler), Profile::FromWebUI(web_ui()),
          web_ui()->GetWebContents());
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

CertificateManagerUI::~CertificateManagerUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(CertificateManagerUI)
