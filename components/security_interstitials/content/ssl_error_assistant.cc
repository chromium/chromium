// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/ssl_error_assistant.h"

#include <memory>

#include "build/build_config.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

net::CertStatus MapToCertStatus(
    chrome_browser_ssl::DynamicInterstitial::CertError error) {
  switch (error) {
    case chrome_browser_ssl::DynamicInterstitial::ERR_CERT_COMMON_NAME_INVALID:
      return net::CERT_STATUS_COMMON_NAME_INVALID;
    case chrome_browser_ssl::DynamicInterstitial::ERR_CERT_DATE_INVALID:
      return net::CERT_STATUS_DATE_INVALID;
    case chrome_browser_ssl::DynamicInterstitial::ERR_CERT_AUTHORITY_INVALID:
      return net::CERT_STATUS_AUTHORITY_INVALID;
    case chrome_browser_ssl::DynamicInterstitial::
        ERR_CERT_NO_REVOCATION_MECHANISM:
      return net::CERT_STATUS_NO_REVOCATION_MECHANISM;
    case chrome_browser_ssl::DynamicInterstitial::
        ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
      return net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
    case chrome_browser_ssl::DynamicInterstitial::
        ERR_CERTIFICATE_TRANSPARENCY_REQUIRED:
      return net::CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED;
    case chrome_browser_ssl::DynamicInterstitial::
        ERR_CERT_KNOWN_INTERCEPTION_BLOCKED:
      return net::CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED;
    case chrome_browser_ssl::DynamicInterstitial::ERR_CERT_SYMANTEC_LEGACY:
      return net::CERT_STATUS_SYMANTEC_LEGACY;
    case chrome_browser_ssl::DynamicInterstitial::ERR_CERT_REVOKED:
      return net::CERT_STATUS_REVOKED;
    case chrome_browser_ssl::DynamicInterstitial::ERR_CERT_INVALID:
      return net::CERT_STATUS_INVALID;
    case chrome_browser_ssl::DynamicInterstitial::
        ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
      return net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM;
    case chrome_browser_ssl::DynamicInterstitial::ERR_CERT_NON_UNIQUE_NAME:
      return net::CERT_STATUS_NON_UNIQUE_NAME;
    case chrome_browser_ssl::DynamicInterstitial::ERR_CERT_WEAK_KEY:
      return net::CERT_STATUS_WEAK_KEY;
    case chrome_browser_ssl::DynamicInterstitial::
        ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN:
      return net::CERT_STATUS_PINNED_KEY_MISSING;
    case chrome_browser_ssl::DynamicInterstitial::
        ERR_CERT_NAME_CONSTRAINT_VIOLATION:
      return net::CERT_STATUS_NAME_CONSTRAINT_VIOLATION;
    case chrome_browser_ssl::DynamicInterstitial::ERR_CERT_VALIDITY_TOO_LONG:
      return net::CERT_STATUS_VALIDITY_TOO_LONG;
    default:
      return 0;
  }
}

std::unordered_set<std::string> HashesFromDynamicInterstitial(
    const chrome_browser_ssl::DynamicInterstitial& entry) {
  std::unordered_set<std::string> hashes;
  for (const std::string& hash : entry.sha256_hash())
    hashes.insert(hash);

  return hashes;
}

std::unique_ptr<std::unordered_set<std::string>> LoadCaptivePortalCertHashes(
    const chrome_browser_ssl::SSLErrorAssistantConfig& proto) {
  auto hashes = std::make_unique<std::unordered_set<std::string>>();
  for (const chrome_browser_ssl::CaptivePortalCert& cert :
       proto.captive_portal_cert()) {
    hashes.get()->insert(cert.sha256_hash());
  }
  return hashes;
}

std::unique_ptr<std::vector<MITMSoftwareType>> LoadMITMSoftwareList(
    const chrome_browser_ssl::SSLErrorAssistantConfig& proto) {
  auto mitm_software_list = std::make_unique<std::vector<MITMSoftwareType>>();

  for (const chrome_browser_ssl::MITMSoftware& proto_entry :
       proto.mitm_software()) {
    // The |name| field and at least one of the |issuer_common_name_regex| and
    // |issuer_organization_regex| fields must be set.
    DCHECK(!proto_entry.name().empty());
    DCHECK(!(proto_entry.issuer_common_name_regex().empty() &&
             proto_entry.issuer_organization_regex().empty()));
    if (proto_entry.name().empty() ||
        (proto_entry.issuer_common_name_regex().empty() &&
         proto_entry.issuer_organization_regex().empty())) {
      continue;
    }

    mitm_software_list.get()->push_back(MITMSoftwareType(
        proto_entry.name(), proto_entry.issuer_common_name_regex(),
        proto_entry.issuer_organization_regex()));
  }
  return mitm_software_list;
}

std::unique_ptr<std::vector<DynamicInterstitialInfo>>
LoadDynamicInterstitialList(
    const chrome_browser_ssl::SSLErrorAssistantConfig& proto) {
  auto dynamic_interstitial_list =
      std::make_unique<std::vector<DynamicInterstitialInfo>>();
  for (const chrome_browser_ssl::DynamicInterstitial& entry :
       proto.dynamic_interstitial()) {
    dynamic_interstitial_list.get()->push_back(DynamicInterstitialInfo(
        HashesFromDynamicInterstitial(entry), entry.issuer_common_name_regex(),
        entry.issuer_organization_regex(), entry.mitm_software_name(),
        entry.interstitial_type(), MapToCertStatus(entry.cert_error()),
        GURL(entry.support_url()),
        entry.show_only_for_nonoverridable_errors()));
  }

  return dynamic_interstitial_list;
}

bool RegexMatchesAny(const std::vector<std::string>& organization_names,
                     const std::string& pattern) {
  const re2::RE2 regex(pattern);
  for (const std::string& organization_name : organization_names) {
    if (re2::RE2::FullMatch(organization_name, regex)) {
      return true;
    }
  }
  return false;
}

// Returns true if a hash in |ssl_info| is found in |spki_hashes|, a list of
// hashes.
bool MatchSSLInfoWithHashes(const net::SSLInfo& ssl_info,
                            std::unordered_set<std::string> spki_hashes) {
  for (const net::HashValue& hash_value : ssl_info.public_key_hashes) {
    if (hash_value.tag() != net::HASH_VALUE_SHA256)
      continue;

    if (spki_hashes.find(hash_value.ToString()) != spki_hashes.end())
      return true;
  }

  return false;
}

}  // namespace

MITMSoftwareType::MITMSoftwareType(const std::string& name,
                                   const std::string& issuer_common_name_regex,
                                   const std::string& issuer_organization_regex)
    : name(name),
      issuer_common_name_regex(issuer_common_name_regex),
      issuer_organization_regex(issuer_organization_regex) {}

DynamicInterstitialInfo::DynamicInterstitialInfo(
    const std::unordered_set<std::string>& spki_hashes,
    const std::string& issuer_common_name_regex,
    const std::string& issuer_organization_regex,
    const std::string& mitm_software_name,
    chrome_browser_ssl::DynamicInterstitial::InterstitialPageType
        interstitial_type,
    int cert_error,
    const GURL& support_url,
    bool show_only_for_nonoverridable_errors)
    : spki_hashes(spki_hashes),
      issuer_common_name_regex(issuer_common_name_regex),
      issuer_organization_regex(issuer_organization_regex),
      mitm_software_name(mitm_software_name),
      interstitial_type(interstitial_type),
      cert_error(cert_error),
      support_url(support_url),
      show_only_for_nonoverridable_errors(show_only_for_nonoverridable_errors) {
}

DynamicInterstitialInfo::~DynamicInterstitialInfo() = default;

DynamicInterstitialInfo::DynamicInterstitialInfo(
    const DynamicInterstitialInfo& other) = default;

SSLErrorAssistant::SSLErrorAssistant() = default;

SSLErrorAssistant::~SSLErrorAssistant() = default;

bool SSLErrorAssistant::IsKnownCaptivePortalCertificate(
    const net::SSLInfo& ssl_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  EnsureInitialized();
  CHECK(error_assistant_proto_);

  if (!captive_portal_spki_hashes_) {
    captive_portal_spki_hashes_ =
        LoadCaptivePortalCertHashes(*error_assistant_proto_);
  }

  return MatchSSLInfoWithHashes(ssl_info, *(captive_portal_spki_hashes_.get()));
}

std::optional<DynamicInterstitialInfo>
SSLErrorAssistant::MatchDynamicInterstitial(const net::SSLInfo& ssl_info,
                                            bool is_overridable) {
  // Load the dynamic interstitial data from SSL error assistant proto if it's
  // not already loaded.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  EnsureInitialized();
  CHECK(error_assistant_proto_);

  if (!dynamic_interstitial_list_) {
    DCHECK(error_assistant_proto_);
    dynamic_interstitial_list_ =
        LoadDynamicInterstitialList(*error_assistant_proto_);
  }

  for (const DynamicInterstitialInfo& data : *dynamic_interstitial_list_) {
    if (data.cert_error && !(ssl_info.cert_status & data.cert_error))
      continue;

    if (!data.spki_hashes.empty() &&
        !MatchSSLInfoWithHashes(ssl_info, data.spki_hashes)) {
      continue;
    }

    if (!data.issuer_common_name_regex.empty() &&
        !re2::RE2::FullMatch(ssl_info.cert->issuer().common_name,
                             re2::RE2(data.issuer_common_name_regex))) {
      continue;
    }

    if (!data.issuer_organization_regex.empty() &&
        !RegexMatchesAny(ssl_info.cert->issuer().organization_names,
                         data.issuer_organization_regex)) {
      continue;
    }

    // Don't match the entry if it's only intended for non-overridable
    // errors, but the current error is overridable.
    if (data.show_only_for_nonoverridable_errors && is_overridable) {
      continue;
    }

    return data;
  }

  return std::nullopt;
}

const std::string SSLErrorAssistant::MatchKnownMITMSoftware(
    const scoped_refptr<net::X509Certificate>& cert) {
  // Ignore if the certificate doesn't have an issuer common name or an
  // organization name.
  if (cert->issuer().common_name.empty() &&
      cert->issuer().organization_names.empty()) {
    return std::string();
  }

  EnsureInitialized();
  CHECK(error_assistant_proto_);

  // Load MITM software data from the SSL error assistant proto.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!mitm_software_list_) {
    DCHECK(error_assistant_proto_);
    mitm_software_list_ = LoadMITMSoftwareList(*error_assistant_proto_);
  }

  for (const MITMSoftwareType& mitm_software : *mitm_software_list_) {
    // At least one of the common name or organization name fields should be
    // populated on the MITM software list entry.
    DCHECK(!(mitm_software.issuer_common_name_regex.empty() &&
             mitm_software.issuer_organization_regex.empty()));
    if (mitm_software.issuer_common_name_regex.empty() &&
        mitm_software.issuer_organization_regex.empty()) {
      continue;
    }

    // If both |issuer_common_name_regex| and |issuer_organization_regex| are
    // set, the certificate should match both regexes.
    if (!mitm_software.issuer_common_name_regex.empty() &&
        !mitm_software.issuer_organization_regex.empty()) {
      if (re2::RE2::FullMatch(
              cert->issuer().common_name,
              re2::RE2(mitm_software.issuer_common_name_regex)) &&
          RegexMatchesAny(cert->issuer().organization_names,
                          mitm_software.issuer_organization_regex)) {
        return mitm_software.name;
      }

      // If only |issuer_organization_regex| is set, the certificate's issuer
      // organization name should match.
    } else if (!mitm_software.issuer_organization_regex.empty()) {
      if (RegexMatchesAny(cert->issuer().organization_names,
                          mitm_software.issuer_organization_regex)) {
        return mitm_software.name;
      }

      // If only |issuer_common_name_regex| is set, the certificate's issuer
      // common name should match.
    } else if (!mitm_software.issuer_common_name_regex.empty()) {
      if (re2::RE2::FullMatch(
              cert->issuer().common_name,
              re2::RE2(mitm_software.issuer_common_name_regex))) {
        return mitm_software.name;
      }
    }
  }
  return std::string();
}

// static
std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
SSLErrorAssistant::GetErrorAssistantProtoFromResourceBundle() {
  // Reads the SSL error assistant configuration from the resource bundle.
  auto proto = std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  DCHECK(proto);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::string data = bundle.LoadDataResourceString(IDR_SSL_ERROR_ASSISTANT_PB);
  return proto->ParseFromString(data) ? std::move(proto) : nullptr;
}

void SSLErrorAssistant::SetErrorAssistantProto(
    std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> proto) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(proto);

  // Ignore versions that are not new. INT_MAX is used by tests, so always allow
  // it.
  if (error_assistant_proto_ && proto->version_id() != INT_MAX &&
      proto->version_id() <= error_assistant_proto_->version_id()) {
    return;
  }

  error_assistant_proto_ = std::move(proto);

  mitm_software_list_ = LoadMITMSoftwareList(*error_assistant_proto_);

  captive_portal_spki_hashes_ =
      LoadCaptivePortalCertHashes(*error_assistant_proto_);

  dynamic_interstitial_list_ =
      LoadDynamicInterstitialList(*error_assistant_proto_);
}

void SSLErrorAssistant::EnsureInitialized() {
  if (!error_assistant_proto_)
    error_assistant_proto_ = GetErrorAssistantProtoFromResourceBundle();
}

void SSLErrorAssistant::ResetForTesting() {
  error_assistant_proto_.reset();
  mitm_software_list_.reset();
  captive_portal_spki_hashes_.reset();
  dynamic_interstitial_list_.reset();
}

int SSLErrorAssistant::GetErrorAssistantProtoVersionIdForTesting() const {
  return error_assistant_proto_->version_id();
}
