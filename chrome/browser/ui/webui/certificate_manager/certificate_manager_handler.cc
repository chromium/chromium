// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_utils.h"
#include "chrome/browser/ui/webui/certificate_manager/chrome_root_store_cert_source.h"
#include "chrome/browser/ui/webui/certificate_manager/client_cert_sources.h"
#include "chrome/browser/ui/webui/certificate_manager/enterprise_cert_sources.h"
#include "chrome/browser/ui/webui/certificate_manager/platform_cert_sources.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#endif

namespace {

void GetCertManagementMetadataAsync(
    ProfileNetworkContextService::CertificatePoliciesForView policies,
    CertificateManagerPageHandler::GetCertManagementMetadataCallback callback,
    cert_verifier::mojom::PlatformRootStoreInfoPtr info) {
  certificate_manager_v2::mojom::CertManagementMetadataPtr metadata =
      certificate_manager_v2::mojom::CertManagementMetadata::New();
#if !BUILDFLAG(IS_CHROMEOS)
  metadata->include_system_trust_store =
      policies.certificate_policies->include_system_trust_store;
  metadata->is_include_system_trust_store_managed =
      policies.is_include_system_trust_store_managed;
#else
  // TODO(crbug.com/40928765): figure out how this should be displayed for
  // ChromeOS
  metadata->include_system_trust_store = true;
#endif

  metadata->num_policy_certs =
      policies.full_distrusted_certs.size() +
      policies.certificate_policies->trust_anchors.size() +
      policies.certificate_policies->trust_anchors_with_enforced_constraints
          .size() +
      policies.certificate_policies->trust_anchors_with_additional_constraints
          .size() +
      policies.certificate_policies->all_certificates.size();

  metadata->num_user_added_system_certs = info->user_added_certs.size();

  std::move(callback).Run(std::move(metadata));
}

}  // namespace

CertificateManagerPageHandler::CertificateManagerPageHandler(
    mojo::PendingRemote<certificate_manager_v2::mojom::CertificateManagerPage>
        pending_client,
    mojo::PendingReceiver<
        certificate_manager_v2::mojom::CertificateManagerPageHandler>
        pending_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : remote_client_(std::move(pending_client)),
      handler_(this, std::move(pending_handler)),
      profile_(profile),
      web_contents_(web_contents) {}

CertificateManagerPageHandler::~CertificateManagerPageHandler() = default;

void CertificateManagerPageHandler::GetCertificates(
    certificate_manager_v2::mojom::CertificateSource source_id,
    GetCertificatesCallback callback) {
  GetCertSource(source_id).GetCertificateInfos(std::move(callback));
}

void CertificateManagerPageHandler::ViewCertificate(
    certificate_manager_v2::mojom::CertificateSource source_id,
    const std::string& sha256hash_hex) {
  GetCertSource(source_id).ViewCertificate(sha256hash_hex,
                                           web_contents_->GetWeakPtr());
}

void CertificateManagerPageHandler::ExportCertificates(
    certificate_manager_v2::mojom::CertificateSource source_id) {
  GetCertSource(source_id).ExportCertificates(web_contents_->GetWeakPtr());
}

void CertificateManagerPageHandler::ImportCertificate(
    certificate_manager_v2::mojom::CertificateSource source_id,
    ImportCertificateCallback callback) {
  GetCertSource(source_id).ImportCertificate(web_contents_->GetWeakPtr(),
                                             std::move(callback));
}

void CertificateManagerPageHandler::ImportAndBindCertificate(
    certificate_manager_v2::mojom::CertificateSource source_id,
    ImportCertificateCallback callback) {
  GetCertSource(source_id).ImportAndBindCertificate(web_contents_->GetWeakPtr(),
                                                    std::move(callback));
}

void CertificateManagerPageHandler::DeleteCertificate(
    certificate_manager_v2::mojom::CertificateSource source_id,
    const std::string& sha256hash_hex,
    DeleteCertificateCallback callback) {
  GetCertSource(source_id).DeleteCertificate(sha256hash_hex,
                                             std::move(callback));
}

CertificateManagerPageHandler::CertSource::~CertSource() = default;

CertificateManagerPageHandler::CertSource&
CertificateManagerPageHandler::GetCertSource(
    certificate_manager_v2::mojom::CertificateSource source) {
  std::unique_ptr<CertSource>& source_ptr =
      cert_source_[static_cast<unsigned>(source)];
  if (!source_ptr) {
    switch (source) {
      case certificate_manager_v2::mojom::CertificateSource::kChromeRootStore:
        source_ptr = std::make_unique<ChromeRootStoreCertSource>();
        break;
      case certificate_manager_v2::mojom::CertificateSource::
          kPlatformClientCert:
        source_ptr = CreatePlatformClientCertSource(&remote_client_, profile_);
        break;
      case certificate_manager_v2::mojom::CertificateSource::
          kEnterpriseTrustedCerts:
        source_ptr = std::make_unique<EnterpriseTrustedCertSource>(profile_);
        break;
      case certificate_manager_v2::mojom::CertificateSource::
          kEnterpriseIntermediateCerts:
        source_ptr =
            std::make_unique<EnterpriseIntermediateCertSource>(profile_);
        break;
      case certificate_manager_v2::mojom::CertificateSource::
          kEnterpriseDistrustedCerts:
        source_ptr = std::make_unique<EnterpriseDistrustedCertSource>(profile_);
        break;
      case certificate_manager_v2::mojom::CertificateSource::
          kPlatformUserTrustedCerts:
        source_ptr = std::make_unique<PlatformCertSource>(
            "trusted_certs", cert_verifier::mojom::CertificateTrust::kTrusted);
        break;
      case certificate_manager_v2::mojom::CertificateSource::
          kPlatformUserIntermediateCerts:
        source_ptr = std::make_unique<PlatformCertSource>(
            "intermediate_certs",
            cert_verifier::mojom::CertificateTrust::kUnspecified);
        break;
      case certificate_manager_v2::mojom::CertificateSource::
          kPlatformUserDistrustedCerts:
        source_ptr = std::make_unique<PlatformCertSource>(
            "distrusted_certs",
            cert_verifier::mojom::CertificateTrust::kDistrusted);
        break;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      case certificate_manager_v2::mojom::CertificateSource::
          kProvisionedClientCert:
        source_ptr = CreateProvisionedClientCertSource(profile_);
        break;
#endif
#if BUILDFLAG(IS_CHROMEOS)
      case certificate_manager_v2::mojom::CertificateSource::
          kExtensionsClientCert:
        source_ptr = CreateExtensionsClientCertSource(profile_);
        break;
#endif
    }
  }
  return *source_ptr;
}

void CertificateManagerPageHandler::GetCertManagementMetadata(
    GetCertManagementMetadataCallback callback) {
  ProfileNetworkContextService* service =
      ProfileNetworkContextServiceFactory::GetForContext(profile_);
  ProfileNetworkContextService::CertificatePoliciesForView policies =
      service->GetCertificatePolicyForView();
  content::GetCertVerifierServiceFactory()->GetPlatformRootStoreInfo(
      base::BindOnce(&GetCertManagementMetadataAsync, std::move(policies),
                     std::move(callback)));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
void CertificateManagerPageHandler::ShowNativeManageCertificates() {
  settings_utils::ShowManageSSLCertificates(web_contents_);
}
#endif

#if !BUILDFLAG(IS_CHROMEOS)
void CertificateManagerPageHandler::SetIncludeSystemTrustStore(bool include) {
  auto* prefs = profile_->GetPrefs();
  prefs->SetBoolean(prefs::kCAPlatformIntegrationEnabled, include);
}
#endif

void CertificateManagerPageHandler::CertSource::ImportCertificate(
    base::WeakPtr<content::WebContents> web_contents,
    CertificateManagerPageHandler::ImportCertificateCallback callback) {
  std::move(callback).Run(
      certificate_manager_v2::mojom::ActionResult::NewError("not implemented"));
}

void CertificateManagerPageHandler::CertSource::ImportAndBindCertificate(
    base::WeakPtr<content::WebContents> web_contents,
    CertificateManagerPageHandler::ImportCertificateCallback callback) {
  std::move(callback).Run(
      certificate_manager_v2::mojom::ActionResult::NewError("not implemented"));
}

void CertificateManagerPageHandler::CertSource::DeleteCertificate(
    const std::string& sha256hash_hex,
    CertificateManagerPageHandler::DeleteCertificateCallback callback) {
  std::move(callback).Run(
      certificate_manager_v2::mojom::ActionResult::NewError("not implemented"));
}
