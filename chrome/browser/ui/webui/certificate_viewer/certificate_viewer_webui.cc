// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_viewer/certificate_viewer_webui.h"

#include <memory>
#include <string_view>
#include <utility>
#include <variant>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/certificate_dialogs.h"
#include "chrome/browser/ui/webui/certificate_viewer/certificate_viewer_ui.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents.h"
#include "net/base/ip_address.h"
#include "net/cert/x509_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "net/cert/x509_util_nss.h"
#endif

using content::WebContents;
using content::WebUIMessageHandler;

using chrome_browser_server_certificate_database::CertificateTrust;

namespace {

// TODO(crbug.com/40928765): find a good place to put this shared code.
bool MaskFromIPAndPrefixLength(const net::IPAddress& ip,
                               size_t prefix_length,
                               net::IPAddress* mask) {
  if (ip.IsIPv4()) {
    if (!net::IPAddress::CreateIPv4Mask(mask, prefix_length)) {
      return false;
    }
  } else if (ip.IsIPv6()) {
    if (!net::IPAddress::CreateIPv6Mask(mask, prefix_length)) {
      return false;
    }
  } else {
    // Somehow got an IP address that isn't ipv4 or ipv6?
    return false;
  }
  return true;
}

// Parses the |possible_cidr_constraint|, populating |parsed_cidr| and |mask|,
// and then return true.
//
// If |possible_cidr_constraint| did not properly parse, returns false. The
// state of |parsed_cidr| and |mask| in this case is not guaranteed.
bool ParseCIDRConstraint(std::string_view possible_cidr_constraint,
                         net::IPAddress* parsed_cidr,
                         net::IPAddress* mask) {
  size_t prefix_length;
  if (!net::ParseCIDRBlock(possible_cidr_constraint, parsed_cidr,
                           &prefix_length)) {
    return false;
  }
  return MaskFromIPAndPrefixLength(*parsed_cidr, prefix_length, mask);
}

bool CIDREquals(
    const chrome_browser_server_certificate_database::CIDR& cidr_constraint,
    const net::IPAddress& ip,
    const net::IPAddress& mask) {
  return net::MaskPrefixLength(mask) == cidr_constraint.prefix_length() &&
         base::as_byte_span(cidr_constraint.ip()) ==
             base::as_byte_span(ip.bytes());
}

// LINT.IfChange(CertificateTrustType)
std::optional<CertificateTrust::CertificateTrustType> ConvertIntToTrust(
    int trust) {
  switch (trust) {
    case 0:
      return CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED;
    case 1:
      return CertificateTrust::CERTIFICATE_TRUST_TYPE_UNSPECIFIED;
    case 2:
      return CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED;
    default:
      return std::nullopt;
  }
}

int ConvertTrustToInt(CertificateTrust::CertificateTrustType trust) {
  switch (trust) {
    case CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED:
      return 0;
    case CertificateTrust::CERTIFICATE_TRUST_TYPE_UNSPECIFIED:
    case CertificateTrust::CERTIFICATE_TRUST_TYPE_UNKNOWN:
      return 1;
    case CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED:
      return 2;
    default:
      NOTREACHED();
  }
}
// LINT.ThenChange(//chrome/browser/resources/certificate_viewer/certificate_viewer.ts:CertificateTrustType)

// Helper class for building a Value representation of a certificate. The class
// gathers data for a single node of the representation tree and builds a
// `base::Value::Dict` out of that.
class CertNodeBuilder {
 public:
  // Starts the node with "label" set to |label|.
  explicit CertNodeBuilder(std::string_view label);

  // Convenience version: Converts |label_id| to the corresponding resource
  // string, then delegates to the other constructor.
  explicit CertNodeBuilder(int label_id);

  CertNodeBuilder(const CertNodeBuilder&) = delete;
  CertNodeBuilder& operator=(const CertNodeBuilder&) = delete;

  // Builder methods all return |*this| so that they can be chained in single
  // expressions.

  // Sets the "payload.val" field. Call this at most once.
  CertNodeBuilder& Payload(std::string_view payload);

  // Adds |child| in the list keyed "children". Can be called multiple times.
  CertNodeBuilder& Child(base::Value::Dict child);

  // Similar to Child, but if the argument is null, then this does not add
  // anything.
  CertNodeBuilder& ChildIfNotNullopt(std::optional<base::Value::Dict> child);

  // Creates a base::Value::Dict representation of the collected information.
  // Only call this once.
  base::Value::Dict Build();

 private:
  base::Value::Dict node_;
  base::Value::List children_;
  // |built_| is false until Build() is called. Once it is |true|, |node_| and
  // |children_| are no longer valid for use.
  bool built_ = false;
};

CertNodeBuilder::CertNodeBuilder(std::string_view label) {
  node_.Set("label", label);
}

CertNodeBuilder::CertNodeBuilder(int label_id)
    : CertNodeBuilder(l10n_util::GetStringUTF8(label_id)) {}

CertNodeBuilder& CertNodeBuilder::Payload(std::string_view payload) {
  DCHECK(!node_.FindByDottedPath("payload.val"));
  node_.SetByDottedPath("payload.val", payload);
  return *this;
}

CertNodeBuilder& CertNodeBuilder::Child(base::Value::Dict child) {
  children_.Append(std::move(child));
  return *this;
}

CertNodeBuilder& CertNodeBuilder::ChildIfNotNullopt(
    std::optional<base::Value::Dict> child) {
  if (child) {
    return Child(std::move(*child));
  }
  return *this;
}

base::Value::Dict CertNodeBuilder::Build() {
  DCHECK(!built_);
  if (!children_.empty()) {
    node_.Set("children", std::move(children_));
  }
  built_ = true;
  return std::move(node_);
}

std::string HandleOptionalOrError(
    const x509_certificate_model::OptionalStringOrError& s) {
  if (std::holds_alternative<x509_certificate_model::Error>(s)) {
    return l10n_util::GetStringUTF8(IDS_CERT_DUMP_ERROR);
  } else if (std::holds_alternative<x509_certificate_model::NotPresent>(s)) {
    return l10n_util::GetStringUTF8(IDS_CERT_INFO_FIELD_NOT_PRESENT);
  }
  return std::get<std::string>(s);
}

base::Value::List GenerateConstraintList(
    const chrome_browser_server_certificate_database::Constraints constraints) {
  base::Value::List list;
  for (const auto& dns_constraint : constraints.dns_names()) {
    list.Append(base::Value(dns_constraint));
  }
  for (const auto& cidr_constraint : constraints.cidrs()) {
    net::IPAddress ip(base::as_byte_span(cidr_constraint.ip()));
    if (!ip.IsValid()) {
      continue;
    }
    std::string cidr_string = ip.ToString();
    cidr_string += "/";
    cidr_string += base::NumberToString(cidr_constraint.prefix_length());
    list.Append(base::Value(std::move(cidr_string)));
  }

  return list;
}

std::string DialogArgsForCertList(
    const std::vector<x509_certificate_model::X509CertificateModel>& certs,
    const std::optional<
        chrome_browser_server_certificate_database::CertificateMetadata>&
        cert_metadata,
    CertMetadataModificationsCallback modifications_callback) {
  std::string data;

  // Certificate information. The keys in this dictionary's general key
  // correspond to the IDs in the Html page.
  base::Value::Dict cert_info;
  const x509_certificate_model::X509CertificateModel& model = certs.front();

  cert_info.Set("isError", !model.is_valid());
  cert_info.SetByDottedPath(
      "general.title",
      l10n_util::GetStringFUTF8(IDS_CERT_INFO_DIALOG_TITLE,
                                base::UTF8ToUTF16(model.GetTitle())));

  if (model.is_valid()) {
    // Standard certificate details.
    const std::string alternative_text =
        l10n_util::GetStringUTF8(IDS_CERT_INFO_FIELD_NOT_PRESENT);

    // Issued to information.
    cert_info.SetByDottedPath(
        "general.issued-cn",
        HandleOptionalOrError(model.GetSubjectCommonName()));
    cert_info.SetByDottedPath("general.issued-o",
                              HandleOptionalOrError(model.GetSubjectOrgName()));
    cert_info.SetByDottedPath(
        "general.issued-ou",
        HandleOptionalOrError(model.GetSubjectOrgUnitName()));

    // Issuer information.
    cert_info.SetByDottedPath(
        "general.issuer-cn",
        HandleOptionalOrError(model.GetIssuerCommonName()));
    cert_info.SetByDottedPath("general.issuer-o",
                              HandleOptionalOrError(model.GetIssuerOrgName()));
    cert_info.SetByDottedPath(
        "general.issuer-ou",
        HandleOptionalOrError(model.GetIssuerOrgUnitName()));

    // Validity period.
    base::Time issued, expires;
    std::string issued_str, expires_str;
    if (model.GetTimes(&issued, &expires)) {
      issued_str =
          base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(issued));
      expires_str =
          base::UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(expires));
    } else {
      issued_str = alternative_text;
      expires_str = alternative_text;
    }
    cert_info.SetByDottedPath("general.issue-date", issued_str);
    cert_info.SetByDottedPath("general.expiry-date", expires_str);
    cert_info.SetByDottedPath("general.spki", model.HashSpkiSHA256());
  }

  // We always have a cert hash. We don't always have a SPKI hash, if the cert
  // is not valid.
  cert_info.SetByDottedPath("general.sha256", model.HashCertSHA256());

  // Certificate hierarchy is constructed from bottom up.
  base::Value::List children;
  int index = 0;
  for (const auto& cert : certs) {
    base::Value::Dict cert_node;
    cert_node.Set("label", base::Value(cert.GetTitle()));
    cert_node.SetByDottedPath("payload.index", base::Value(index));
    // Add the child from the previous iteration.
    if (!children.empty()) {
      cert_node.Set("children", std::move(children));
    }

    // Add this node to the children list for the next iteration.
    children = base::Value::List();
    children.Append(std::move(cert_node));
    ++index;
  }
  // Set the last node as the top of the certificate hierarchy.
  cert_info.Set("hierarchy", std::move(children));

  if (cert_metadata.has_value()) {
    base::Value::Dict dict;
    dict.Set(
        "trust",
        base::Value(ConvertTrustToInt(cert_metadata->trust().trust_type())));
    dict.Set("isEditable", base::Value(!modifications_callback.is_null()));
    if (cert_metadata->has_constraints()) {
      dict.Set("constraints",
               GenerateConstraintList(cert_metadata->constraints()));
    }
    cert_info.Set("certMetadata", std::move(dict));
  }

  base::JSONWriter::Write(cert_info, &data);

  return data;
}

}  // namespace

// Shows a certificate using the WebUI certificate viewer.
void ShowCertificateViewer(WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  std::vector<std::string> nicknames;
#if BUILDFLAG(USE_NSS_CERTS)
  net::ScopedCERTCertificateList nss_certs =
      net::x509_util::CreateCERTCertificateListFromX509Certificate(cert);
  // If any of the certs could not be parsed by NSS, |nss_certs| will be an
  // empty list and |nicknames| will not be populated, which is fine as a
  // fallback.
  for (const auto& nss_cert : nss_certs) {
    nicknames.push_back(x509_certificate_model::GetRawNickname(nss_cert.get()));
  }
#endif

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> cert_buffers;
  cert_buffers.push_back(bssl::UpRef(cert->cert_buffer()));
  for (const auto& intermediate : cert->intermediate_buffers()) {
    cert_buffers.push_back(bssl::UpRef(intermediate));
  }
  CertificateViewerDialog::ShowConstrained(
      std::move(cert_buffers), std::move(nicknames), web_contents, parent);
}

#if !(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
void ShowCertificateViewerForClientAuth(content::WebContents* web_contents,
                                        gfx::NativeWindow parent,
                                        net::X509Certificate* cert) {
  ShowCertificateViewer(web_contents, parent, cert);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// CertificateViewerDialog

#if BUILDFLAG(USE_NSS_CERTS)
// static
CertificateViewerDialog* CertificateViewerDialog::ShowConstrained(
    net::ScopedCERTCertificateList nss_certs,
    WebContents* web_contents,
    gfx::NativeWindow parent) {
  std::vector<std::string> nicknames;
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> cert_buffers;
  for (const auto& cert : nss_certs) {
    nicknames.push_back(x509_certificate_model::GetRawNickname(cert.get()));
    cert_buffers.push_back(net::x509_util::CreateCryptoBuffer(
        net::x509_util::CERTCertificateAsSpan(cert.get())));
  }
  return ShowConstrained(std::move(cert_buffers), std::move(nicknames),
                         web_contents, parent);
}
#endif

// static
CertificateViewerDialog* CertificateViewerDialog::ShowConstrained(
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs,
    std::vector<std::string> cert_nicknames,
    content::WebContents* web_contents,
    gfx::NativeWindow parent) {
  return ShowConstrained(std::move(certs), std::move(cert_nicknames),
                         std::nullopt, base::NullCallback(), web_contents,
                         parent);
}

// static
CertificateViewerDialog* CertificateViewerDialog::ShowConstrained(
    bssl::UniquePtr<CRYPTO_BUFFER> cert,
    content::WebContents* web_contents,
    gfx::NativeWindow parent) {
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs;
  certs.push_back(std::move(cert));
  return ShowConstrained(std::move(certs),
                         /*cert_nicknames=*/{}, std::nullopt,
                         base::NullCallback(), web_contents, parent);
}

// static
CertificateViewerDialog* CertificateViewerDialog::ShowConstrainedWithMetadata(
    bssl::UniquePtr<CRYPTO_BUFFER> cert,
    chrome_browser_server_certificate_database::CertificateMetadata
        cert_metadata,
    CertMetadataModificationsCallback modifications_callback,
    content::WebContents* web_contents,
    gfx::NativeWindow parent) {
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs;
  certs.push_back(std::move(cert));

  return CertificateViewerDialog::ShowConstrained(
      std::move(certs), /*cert_nicknames=*/{}, std::move(cert_metadata),
      std::move(modifications_callback), web_contents, parent);
}

// static
CertificateViewerDialog* CertificateViewerDialog::ShowConstrained(
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs,
    std::vector<std::string> cert_nicknames,
    std::optional<
        chrome_browser_server_certificate_database::CertificateMetadata>
        cert_metadata,
    CertMetadataModificationsCallback modifications_callback,
    content::WebContents* web_contents,
    gfx::NativeWindow parent) {
  CertificateViewerDialog* dialog_ptr = new CertificateViewerDialog(
      std::move(certs), std::move(cert_nicknames), std::move(cert_metadata),
      std::move(modifications_callback));

  auto dialog = base::WrapUnique(dialog_ptr);

  // TODO(bshe): UI tweaks needed for Aura HTML Dialog, such as adding padding
  // on the title for Aura ConstrainedWebDialogUI.
  dialog_ptr->delegate_ = ShowConstrainedWebDialog(
      web_contents->GetBrowserContext(), std::move(dialog), web_contents);

  // Clear the zoom level for the dialog so that it is not affected by the page
  // zoom setting.
  content::WebContents* dialog_web_contents =
      dialog_ptr->delegate_->GetWebContents();
  const GURL dialog_url = dialog_ptr->GetDialogContentURL();
  content::HostZoomMap::Get(dialog_web_contents->GetSiteInstance())
      ->SetZoomLevelForHostAndScheme(dialog_url.scheme(), dialog_url.host(), 0);
  return dialog_ptr;  // For tests.
}

gfx::NativeWindow CertificateViewerDialog::GetNativeWebContentsModalDialog() {
  return delegate_->GetNativeDialog();
}

CertificateViewerDialog::CertificateViewerDialog(
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> in_certs,
    std::vector<std::string> cert_nicknames,
    std::optional<
        chrome_browser_server_certificate_database::CertificateMetadata>
        cert_metadata,
    CertMetadataModificationsCallback modifications_callback) {
  CHECK(!in_certs.empty());
  if (cert_metadata) {
    CHECK(in_certs.size() == 1);
  }

  std::vector<x509_certificate_model::X509CertificateModel> certs;
  for (size_t i = 0; i < in_certs.size(); ++i) {
    std::string nickname;
    if (i < cert_nicknames.size()) {
      nickname = std::move(cert_nicknames[i]);
    }
    certs.emplace_back(std::move(in_certs[i]), std::move(nickname));
  }

  constexpr gfx::Size kDefaultSize{544, 628};
  set_can_close(true);
  set_delete_on_close(false);
  set_dialog_args(
      DialogArgsForCertList(certs, cert_metadata, modifications_callback));
  set_dialog_modal_type(ui::mojom::ModalType::kNone);
  set_dialog_content_url(GURL(chrome::kChromeUICertificateViewerURL));
  set_dialog_size(kDefaultSize);
  set_dialog_title(l10n_util::GetStringFUTF16(
      IDS_CERT_INFO_DIALOG_TITLE, base::UTF8ToUTF16(certs.front().GetTitle())));
  set_show_dialog_title(true);

  AddWebUIMessageHandler(std::make_unique<CertificateViewerDialogHandler>(
      this, std::move(certs), std::move(cert_metadata),
      std::move(modifications_callback)));
}

CertificateViewerDialog::~CertificateViewerDialog() = default;

////////////////////////////////////////////////////////////////////////////////
// CertificateViewerDialogHandler

CertificateViewerDialogHandler::CertificateViewerDialogHandler(
    CertificateViewerDialog* dialog,
    std::vector<x509_certificate_model::X509CertificateModel> certs,
    std::optional<
        chrome_browser_server_certificate_database::CertificateMetadata>
        cert_metadata,
    CertMetadataModificationsCallback modifications_callback)
    : dialog_(dialog),
      certs_(std::move(certs)),
      cert_metadata_(std::move(cert_metadata)),
      modifications_callback_(std::move(modifications_callback)) {}

CertificateViewerDialogHandler::~CertificateViewerDialogHandler() = default;

void CertificateViewerDialogHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "exportCertificate",
      base::BindRepeating(
          &CertificateViewerDialogHandler::HandleExportCertificate,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestCertificateFields",
      base::BindRepeating(
          &CertificateViewerDialogHandler::HandleRequestCertificateFields,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateTrustState",
      base::BindRepeating(
          &CertificateViewerDialogHandler::HandleUpdateTrustState,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "addConstraint",
      base::BindRepeating(&CertificateViewerDialogHandler::HandleAddConstraint,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "deleteConstraint",
      base::BindRepeating(
          &CertificateViewerDialogHandler::HandleDeleteConstraint,
          base::Unretained(this)));
}

void CertificateViewerDialogHandler::HandleExportCertificate(
    const base::Value::List& args) {
  int cert_index = GetCertificateIndex(args[0].GetInt());
  if (cert_index < 0) {
    return;
  }

  gfx::NativeWindow window =
      platform_util::GetTopLevel(platform_util::GetViewForWindow(
          dialog_->GetNativeWebContentsModalDialog()));

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> export_certs;
  for (const auto& cert :
       base::span(certs_).subspan(static_cast<size_t>(cert_index))) {
    export_certs.push_back(bssl::UpRef(cert.cert_buffer()));
  }
  ShowCertExportDialog(web_ui()->GetWebContents(), window,
                       std::move(export_certs), certs_[cert_index].GetTitle());
}

bool CertificateViewerDialogHandler::CanModifyMetadata() const {
  // To be able to modify the Cert Metadata, we need:
  //   - non-null modifications_callback_
  //   - exactly one cert
  //   - cert metadata
  //
  // In normal circumstances these should always be true when this function is
  // called, barring any bugs or if the user is messing with the HTML/JS.
  return modifications_callback_ && certs_.size() == 1 && cert_metadata_;
}

void CertificateViewerDialogHandler::HandleUpdateTrustState(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  std::optional<CertificateTrust::CertificateTrustType> trust_opt =
      ConvertIntToTrust(args[1].GetInt());
  // Trust type is invalid for some reason. Should only happen if a user is
  // messing with the HTML/JS.
  if (!trust_opt) {
    base::Value::Dict dict;
    dict.Set("success", base::Value(false));
    dict.Set("error", base::Value("An error occured updating the trust state"));
    ResolveJavascriptCallback(callback_id, dict);
    return;
  }

  if (!CanModifyMetadata()) {
    base::Value::Dict dict;
    dict.Set("success", base::Value(false));
    dict.Set("error",
             base::Value("Modification of this certificate is not allowed"));
    ResolveJavascriptCallback(callback_id, dict);
    return;
  }

  net::ServerCertificateDatabase::CertInformation cert_info(
      net::x509_util::CryptoBufferAsSpan(certs_[0].cert_buffer()));
  // Copy current cert metadata, then update with new trust.
  cert_info.cert_metadata.MergeFrom(*cert_metadata_);
  cert_info.cert_metadata.mutable_trust()->set_trust_type(*trust_opt);

  modifications_callback_.Run(
      std::move(cert_info),
      base::BindOnce(&CertificateViewerDialogHandler::UpdateTrustStateDone,
                     weak_ptr_factory_.GetWeakPtr(), callback_id.Clone(),
                     *trust_opt));
}

void CertificateViewerDialogHandler::UpdateTrustStateDone(
    const base::Value& callback_id,
    CertificateTrust::CertificateTrustType new_trust,
    bool success) {
  base::Value::Dict dict;
  dict.Set("success", base::Value(success));
  cert_metadata_->mutable_trust()->set_trust_type(new_trust);
  // No error message set, use the default message.
  ResolveJavascriptCallback(callback_id, dict);
}

void CertificateViewerDialogHandler::HandleDeleteConstraint(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];

  std::string constraint_to_delete = args[1].GetString();

  if (!CanModifyMetadata()) {
    base::Value::Dict dict;
    dict.Set("success", base::Value(false));
    dict.Set("error",
             base::Value("Modification of this certificate is not allowed"));
    ResolveJavascriptCallback(callback_id, dict);
    return;
  }

  net::ServerCertificateDatabase::CertInformation cert_info(
      net::x509_util::CryptoBufferAsSpan(certs_[0].cert_buffer()));
  cert_info.cert_metadata.mutable_trust()->set_trust_type(
      cert_metadata_->trust().trust_type());

  // Try to find the constraint to remove. If there are duplicates, it will
  // remove just the first one.
  bool removed = false;
  // Assume its a DNS constraint, try to remove the constraint from dns names
  // first.
  for (const std::string& constraint :
       cert_metadata_->constraints().dns_names()) {
    // If we already removed an entry, add everything else.
    // Or its not equal, just add it as well.
    if (removed || (constraint != constraint_to_delete)) {
      cert_info.cert_metadata.mutable_constraints()->add_dns_names(constraint);
    } else {
      removed = true;
    }
  }

  if (removed) {
    // Already found the constraint and removed it, add all of the cidr
    // constraints.
    for (const auto& cidr_constraint : cert_metadata_->constraints().cidrs()) {
      cert_info.cert_metadata.mutable_constraints()->add_cidrs()->MergeFrom(
          cidr_constraint);
    }
  } else {
    net::IPAddress parsed_cidr;
    net::IPAddress mask;
    if (ParseCIDRConstraint(constraint_to_delete, &parsed_cidr, &mask)) {
      for (const auto& cidr_constraint :
           cert_metadata_->constraints().cidrs()) {
        if (removed || !CIDREquals(cidr_constraint, parsed_cidr, mask)) {
          cert_info.cert_metadata.mutable_constraints()->add_cidrs()->MergeFrom(
              cidr_constraint);
        } else {
          removed = true;
        }
      }
    }
  }

  if (!removed) {
    base::Value::Dict dict;
    dict.Set("success", base::Value(false));
    dict.Set("error",
             base::Value("Error removing constraint from certificate"));
    ResolveJavascriptCallback(callback_id, dict);
    return;
  }

  chrome_browser_server_certificate_database::Constraints new_constraints(
      cert_info.cert_metadata.constraints());
  modifications_callback_.Run(
      std::move(cert_info),
      base::BindOnce(&CertificateViewerDialogHandler::UpdateConstraintsDone,
                     weak_ptr_factory_.GetWeakPtr(), callback_id.Clone(),
                     std::move(new_constraints)));
}

void CertificateViewerDialogHandler::HandleAddConstraint(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];

  std::string constraint_to_add = args[1].GetString();

  if (!CanModifyMetadata()) {
    base::Value::Dict dict;
    dict.Set("success", base::Value(false));
    dict.Set("error",
             base::Value("Modification of this certificate is not allowed"));
    ResolveJavascriptCallback(callback_id, dict);
    return;
  }

  net::ServerCertificateDatabase::CertInformation cert_info(
      net::x509_util::CryptoBufferAsSpan(certs_[0].cert_buffer()));
  // Copy current cert metadata, then update with extra constraint.
  cert_info.cert_metadata.MergeFrom(*cert_metadata_);

  // Try new constraint as a CIDR block; if it doesn't parse assume its a DNS
  // name.
  //
  // Currently we don't check for duplicates, or validate the DNS name pattern.
  net::IPAddress parsed_cidr;
  net::IPAddress mask;
  if (ParseCIDRConstraint(constraint_to_add, &parsed_cidr, &mask)) {
    chrome_browser_server_certificate_database::CIDR* new_cidr_constraint =
        cert_info.cert_metadata.mutable_constraints()->add_cidrs();
    new_cidr_constraint->set_prefix_length(net::MaskPrefixLength(mask));
    new_cidr_constraint->set_ip(
        std::string(base::as_string_view(parsed_cidr.bytes())));
  } else {
    cert_info.cert_metadata.mutable_constraints()->add_dns_names(
        constraint_to_add);
  }

  chrome_browser_server_certificate_database::Constraints new_constraints(
      cert_info.cert_metadata.constraints());

  modifications_callback_.Run(
      std::move(cert_info),
      base::BindOnce(&CertificateViewerDialogHandler::UpdateConstraintsDone,
                     weak_ptr_factory_.GetWeakPtr(), callback_id.Clone(),
                     std::move(new_constraints)));
}

void CertificateViewerDialogHandler::UpdateConstraintsDone(
    const base::Value& callback_id,
    const chrome_browser_server_certificate_database::Constraints
        new_constraints,
    bool success) {
  base::Value::Dict dict;
  // No error message set, use the default message.
  dict.Set("success", base::Value(success));

  base::Value::Dict result;
  result.Set("status", std::move(dict));
  if (success) {
    cert_metadata_->clear_constraints();
    cert_metadata_->mutable_constraints()->MergeFrom(new_constraints);
    result.Set("constraints", GenerateConstraintList(new_constraints));
  }
  ResolveJavascriptCallback(callback_id, result);
}

void CertificateViewerDialogHandler::HandleRequestCertificateFields(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  int cert_index = GetCertificateIndex(args[1].GetInt());
  if (cert_index < 0) {
    return;
  }

  const x509_certificate_model::X509CertificateModel& model =
      certs_[cert_index];

  CertNodeBuilder contents_builder(IDS_CERT_DETAILS_CERTIFICATE);

  if (model.is_valid()) {
    CertNodeBuilder issued_node_builder(IDS_CERT_DETAILS_NOT_BEFORE);
    CertNodeBuilder expires_node_builder(IDS_CERT_DETAILS_NOT_AFTER);
    base::Time issued, expires;
    if (model.GetTimes(&issued, &expires)) {
      issued_node_builder.Payload(base::UTF16ToUTF8(
          base::TimeFormatShortDateAndTimeWithTimeZone(issued)));
      expires_node_builder.Payload(base::UTF16ToUTF8(
          base::TimeFormatShortDateAndTimeWithTimeZone(expires)));
    }

    std::vector<x509_certificate_model::Extension> extensions =
        model.GetExtensions(
            l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_CRITICAL),
            l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_NON_CRITICAL));

    std::optional<base::Value::Dict> details_extensions;
    if (!extensions.empty()) {
      CertNodeBuilder details_extensions_builder(IDS_CERT_DETAILS_EXTENSIONS);
      for (const x509_certificate_model::Extension& extension : extensions) {
        details_extensions_builder.Child(
            CertNodeBuilder(extension.name).Payload(extension.value).Build());
      }
      details_extensions = details_extensions_builder.Build();
    }

    contents_builder
        // Main certificate fields.
        .Child(CertNodeBuilder(IDS_CERT_DETAILS_VERSION)
                   .Payload(l10n_util::GetStringFUTF8(
                       IDS_CERT_DETAILS_VERSION_FORMAT,
                       base::UTF8ToUTF16(model.GetVersion())))
                   .Build())
        .Child(CertNodeBuilder(IDS_CERT_DETAILS_SERIAL_NUMBER)
                   .Payload(model.GetSerialNumberHexified())
                   .Build())
        .Child(CertNodeBuilder(IDS_CERT_DETAILS_CERTIFICATE_SIG_ALG)
                   .Payload(model.ProcessSecAlgorithmSignature())
                   .Build())
        .Child(CertNodeBuilder(IDS_CERT_DETAILS_ISSUER)
                   .Payload(HandleOptionalOrError(model.GetIssuerName()))
                   .Build())
        // Validity period.
        .Child(CertNodeBuilder(IDS_CERT_DETAILS_VALIDITY)
                   .Child(issued_node_builder.Build())
                   .Child(expires_node_builder.Build())
                   .Build())
        .Child(CertNodeBuilder(IDS_CERT_DETAILS_SUBJECT)
                   .Payload(HandleOptionalOrError(model.GetSubjectName()))
                   .Build())
        // Subject key information.
        .Child(
            CertNodeBuilder(IDS_CERT_DETAILS_SUBJECT_KEY_INFO)
                .Child(CertNodeBuilder(IDS_CERT_DETAILS_SUBJECT_KEY_ALG)
                           .Payload(model.ProcessSecAlgorithmSubjectPublicKey())
                           .Build())
                .Child(CertNodeBuilder(IDS_CERT_DETAILS_SUBJECT_KEY)
                           .Payload(model.ProcessSubjectPublicKeyInfo())
                           .Build())
                .Build())
        // Extensions.
        .ChildIfNotNullopt(std::move(details_extensions))
        .Child(CertNodeBuilder(IDS_CERT_DETAILS_CERTIFICATE_SIG_ALG)
                   .Payload(model.ProcessSecAlgorithmSignatureWrap())
                   .Build())
        .Child(CertNodeBuilder(IDS_CERT_DETAILS_CERTIFICATE_SIG_VALUE)
                   .Payload(model.ProcessRawBitsSignatureWrap())
                   .Build());
  }
  CertNodeBuilder fingerprint_builder =
      CertNodeBuilder(IDS_CERT_INFO_FINGERPRINTS_GROUP);
  fingerprint_builder.Child(
      CertNodeBuilder(IDS_CERT_INFO_SHA256_FINGERPRINT_LABEL)
          .Payload(model.HashCertSHA256())
          .Build());
  if (model.is_valid()) {
    fingerprint_builder.Child(
        CertNodeBuilder(IDS_CERT_INFO_SHA256_SPKI_FINGERPRINT_LABEL)
            .Payload(model.HashSpkiSHA256())
            .Build());
  }
  contents_builder.Child(fingerprint_builder.Build());

  base::Value::List root_list;
  root_list.Append(CertNodeBuilder(model.GetTitle())
                       .Child(contents_builder.Build())
                       .Build());
  // Send certificate information to javascript.
  ResolveJavascriptCallback(callback_id, root_list);
}

int CertificateViewerDialogHandler::GetCertificateIndex(
    int requested_index) const {
  int cert_index = requested_index;
  if (cert_index < 0 || static_cast<size_t>(cert_index) >= certs_.size()) {
    return -1;
  }
  return cert_index;
}
