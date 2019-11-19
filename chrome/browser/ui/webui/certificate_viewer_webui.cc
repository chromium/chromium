// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_viewer_webui.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/certificate_dialogs.h"
#include "chrome/browser/ui/webui/certificate_viewer_ui.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_util_nss.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

// Helper class for building a Value representation of a certificate. The class
// gathers data for a single node of the representation tree and builds a
// DictionaryValue out of that.
class CertNodeBuilder {
 public:
  // Starts the node with "label" set to |label|.
  explicit CertNodeBuilder(base::StringPiece label);

  // Convenience version: Converts |label_id| to the corresponding resource
  // string, then delegates to the other constructor.
  explicit CertNodeBuilder(int label_id);

  // Builder methods all return |*this| so that they can be chained in single
  // expressions.

  // Sets the "payload.val" field. Call this at most once.
  CertNodeBuilder& Payload(base::StringPiece payload);

  // Adds |child| in the list keyed "children". Can be called multiple times.
  CertNodeBuilder& Child(std::unique_ptr<base::DictionaryValue> child);

  // Similar to Child, but if the argument is null, then this does not add
  // anything.
  CertNodeBuilder& ChildIfNotNull(std::unique_ptr<base::DictionaryValue> child);

  // Creates a DictionaryValue representation of the collected information. Only
  // call this once.
  std::unique_ptr<base::DictionaryValue> Build();

 private:
  base::DictionaryValue node_;
  base::ListValue children_;
  // |built_| is false until Build() is called. Once it is |true|, |node_| and
  // |children_| are no longer valid for use.
  bool built_ = false;

  DISALLOW_COPY_AND_ASSIGN(CertNodeBuilder);
};

CertNodeBuilder::CertNodeBuilder(base::StringPiece label) {
  node_.SetString("label", label);
}

CertNodeBuilder::CertNodeBuilder(int label_id)
    : CertNodeBuilder(l10n_util::GetStringUTF8(label_id)) {}

CertNodeBuilder& CertNodeBuilder::Payload(base::StringPiece payload) {
  DCHECK(!node_.HasKey("payload.val"));
  node_.SetString("payload.val", payload);
  return *this;
}

CertNodeBuilder& CertNodeBuilder::Child(
    std::unique_ptr<base::DictionaryValue> child) {
  children_.Append(std::move(child));
  return *this;
}

CertNodeBuilder& CertNodeBuilder::ChildIfNotNull(
    std::unique_ptr<base::DictionaryValue> child) {
  if (child)
    return Child(std::move(child));
  return *this;
}

std::unique_ptr<base::DictionaryValue> CertNodeBuilder::Build() {
  DCHECK(!built_);
  if (!children_.empty()) {
    node_.SetKey("children", std::move(children_));
  }
  built_ = true;
  return std::make_unique<base::DictionaryValue>(std::move(node_));
}

}  // namespace

// Shows a certificate using the WebUI certificate viewer.
void ShowCertificateViewer(WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  net::ScopedCERTCertificateList nss_certs =
      net::x509_util::CreateCERTCertificateListFromX509Certificate(cert);
  if (nss_certs.empty())
    return;

  CertificateViewerDialog::ShowConstrained(std::move(nss_certs), web_contents,
                                           parent);
}

////////////////////////////////////////////////////////////////////////////////
// CertificateViewerDialog

// static
CertificateViewerDialog* CertificateViewerDialog::ShowConstrained(
    net::ScopedCERTCertificateList certs,
    WebContents* web_contents,
    gfx::NativeWindow parent) {
  CertificateViewerDialog* dialog_ptr =
      new CertificateViewerDialog(std::move(certs));
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
    net::ScopedCERTCertificateList certs)
    : nss_certs_(std::move(certs)) {
  // Construct the dialog title from the certificate.
  title_ = l10n_util::GetStringFUTF16(
      IDS_CERT_INFO_DIALOG_TITLE,
      base::UTF8ToUTF16(
          x509_certificate_model::GetTitle(nss_certs_.front().get())));
}

CertificateViewerDialog::~CertificateViewerDialog() = default;

ui::ModalType CertificateViewerDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_NONE;
}

base::string16 CertificateViewerDialog::GetDialogTitle() const {
  return title_;
}

GURL CertificateViewerDialog::GetDialogContentURL() const {
  return GURL(chrome::kChromeUICertificateViewerURL);
}

void CertificateViewerDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(new CertificateViewerDialogHandler(
      const_cast<CertificateViewerDialog*>(this),
      net::x509_util::DupCERTCertificateList(nss_certs_)));
}

void CertificateViewerDialog::GetDialogSize(gfx::Size* size) const {
  const int kDefaultWidth = 544;
  const int kDefaultHeight = 628;
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

std::string CertificateViewerDialog::GetDialogArgs() const {
  std::string data;

  // Certificate information. The keys in this dictionary's general key
  // correspond to the IDs in the Html page.
  base::DictionaryValue cert_info;
  CERTCertificate* cert_hnd = nss_certs_.front().get();

  // Certificate usage.
  std::vector<std::string> usages;
  x509_certificate_model::GetUsageStrings(cert_hnd, &usages);
  cert_info.SetString("general.usages", base::JoinString(usages, "\n"));

  // Standard certificate details.
  const std::string alternative_text =
      l10n_util::GetStringUTF8(IDS_CERT_INFO_FIELD_NOT_PRESENT);
  cert_info.SetString(
      "general.title",
      l10n_util::GetStringFUTF8(
          IDS_CERT_INFO_DIALOG_TITLE,
          base::UTF8ToUTF16(x509_certificate_model::GetTitle(cert_hnd))));

  // Issued to information.
  cert_info.SetString("general.issued-cn",
      x509_certificate_model::GetSubjectCommonName(cert_hnd, alternative_text));
  cert_info.SetString("general.issued-o",
      x509_certificate_model::GetSubjectOrgName(cert_hnd, alternative_text));
  cert_info.SetString("general.issued-ou",
      x509_certificate_model::GetSubjectOrgUnitName(cert_hnd,
                                                    alternative_text));

  // Issuer information.
  cert_info.SetString("general.issuer-cn",
      x509_certificate_model::GetIssuerCommonName(cert_hnd, alternative_text));
  cert_info.SetString("general.issuer-o",
      x509_certificate_model::GetIssuerOrgName(cert_hnd, alternative_text));
  cert_info.SetString("general.issuer-ou",
      x509_certificate_model::GetIssuerOrgUnitName(cert_hnd, alternative_text));

  // Validity period.
  base::Time issued, expires;
  std::string issued_str, expires_str;
  if (x509_certificate_model::GetTimes(cert_hnd, &issued, &expires)) {
    issued_str = base::UTF16ToUTF8(
        base::TimeFormatFriendlyDateAndTime(issued));
    expires_str = base::UTF16ToUTF8(
        base::TimeFormatFriendlyDateAndTime(expires));
  } else {
    issued_str = alternative_text;
    expires_str = alternative_text;
  }
  cert_info.SetString("general.issue-date", issued_str);
  cert_info.SetString("general.expiry-date", expires_str);

  cert_info.SetString("general.sha256",
      x509_certificate_model::HashCertSHA256(cert_hnd));
  cert_info.SetString("general.sha1",
      x509_certificate_model::HashCertSHA1(cert_hnd));

  // Certificate hierarchy is constructed from bottom up.
  base::Value children;
  int index = 0;
  for (const auto& cert : nss_certs_) {
    base::Value cert_node(base::Value::Type::DICTIONARY);
    cert_node.SetKey("label",
                     base::Value(x509_certificate_model::GetTitle(cert.get())));
    cert_node.SetPath({"payload", "index"}, base::Value(index));
    // Add the child from the previous iteration.
    if (!children.is_none())
      cert_node.SetKey("children", std::move(children));

    // Add this node to the children list for the next iteration.
    children = base::Value(base::Value::Type::LIST);
    children.Append(std::move(cert_node));
    ++index;
  }
  // Set the last node as the top of the certificate hierarchy.
  cert_info.SetKey("hierarchy", std::move(children));

  base::JSONWriter::Write(cert_info, &data);

  return data;
}

void CertificateViewerDialog::OnDialogShown(content::WebUI* webui) {
  webui_ = webui;
}

void CertificateViewerDialog::OnDialogClosed(const std::string& json_retval) {
  // Don't |delete this|: owned by the constrained dialog manager.
}

void CertificateViewerDialog::OnCloseContents(WebContents* source,
                                              bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool CertificateViewerDialog::ShouldShowDialogTitle() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// CertificateViewerDialogHandler

CertificateViewerDialogHandler::CertificateViewerDialogHandler(
    CertificateViewerDialog* dialog,
    net::ScopedCERTCertificateList cert_chain)
    : dialog_(dialog), cert_chain_(std::move(cert_chain)) {}

CertificateViewerDialogHandler::~CertificateViewerDialogHandler() {
}

void CertificateViewerDialogHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "exportCertificate",
      base::BindRepeating(&CertificateViewerDialogHandler::ExportCertificate,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestCertificateFields",
      base::BindRepeating(
          &CertificateViewerDialogHandler::RequestCertificateFields,
          base::Unretained(this)));
}

void CertificateViewerDialogHandler::ExportCertificate(
    const base::ListValue* args) {
  int cert_index = GetCertificateIndex(args);
  if (cert_index < 0)
    return;

  gfx::NativeWindow window =
      platform_util::GetTopLevel(dialog_->GetNativeWebContentsModalDialog());
  ShowCertExportDialog(web_ui()->GetWebContents(),
                       window,
                       cert_chain_.begin() + cert_index,
                       cert_chain_.end());
}

void CertificateViewerDialogHandler::RequestCertificateFields(
    const base::ListValue* args) {
  int cert_index = GetCertificateIndex(args);
  if (cert_index < 0)
    return;

  CERTCertificate* cert = cert_chain_[cert_index].get();

  CertNodeBuilder version_node(IDS_CERT_DETAILS_VERSION);
  std::string version = x509_certificate_model::GetVersion(cert);
  if (!version.empty()) {
    version_node.Payload(l10n_util::GetStringFUTF8(
        IDS_CERT_DETAILS_VERSION_FORMAT, base::UTF8ToUTF16(version)));
  }

  CertNodeBuilder issued_node_builder(IDS_CERT_DETAILS_NOT_BEFORE);
  CertNodeBuilder expires_node_builder(IDS_CERT_DETAILS_NOT_AFTER);
  base::Time issued, expires;
  if (x509_certificate_model::GetTimes(cert, &issued, &expires)) {
    issued_node_builder.Payload(base::UTF16ToUTF8(
        base::TimeFormatShortDateAndTimeWithTimeZone(issued)));
    expires_node_builder.Payload(base::UTF16ToUTF8(
        base::TimeFormatShortDateAndTimeWithTimeZone(expires)));
  }

  x509_certificate_model::Extensions extensions;
  x509_certificate_model::GetExtensions(
      l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_CRITICAL),
      l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_NON_CRITICAL),
      cert, &extensions);

  std::unique_ptr<base::DictionaryValue> details_extensions;
  if (!extensions.empty()) {
    CertNodeBuilder details_extensions_builder(IDS_CERT_DETAILS_EXTENSIONS);
    for (const x509_certificate_model::Extension& extension : extensions) {
      details_extensions_builder.Child(
          CertNodeBuilder(extension.name).Payload(extension.value).Build());
    }
    details_extensions = details_extensions_builder.Build();
  }

  base::ListValue root_list;
  root_list.Append(
      CertNodeBuilder(x509_certificate_model::GetTitle(cert))
          .Child(
              CertNodeBuilder(
                  l10n_util::GetStringUTF8(IDS_CERT_DETAILS_CERTIFICATE))
                  // Main certificate fields.
                  .Child(version_node.Build())
                  .Child(
                      CertNodeBuilder(IDS_CERT_DETAILS_SERIAL_NUMBER)
                          .Payload(
                              x509_certificate_model::GetSerialNumberHexified(
                                  cert, l10n_util::GetStringUTF8(
                                            IDS_CERT_INFO_FIELD_NOT_PRESENT)))
                          .Build())
                  .Child(CertNodeBuilder(IDS_CERT_DETAILS_CERTIFICATE_SIG_ALG)
                             .Payload(x509_certificate_model::
                                          ProcessSecAlgorithmSignature(cert))
                             .Build())
                  .Child(
                      CertNodeBuilder(IDS_CERT_DETAILS_ISSUER)
                          .Payload(x509_certificate_model::GetIssuerName(cert))
                          .Build())
                  // Validity period.
                  .Child(CertNodeBuilder(IDS_CERT_DETAILS_VALIDITY)
                             .Child(issued_node_builder.Build())
                             .Child(expires_node_builder.Build())
                             .Build())
                  .Child(
                      CertNodeBuilder(IDS_CERT_DETAILS_SUBJECT)
                          .Payload(x509_certificate_model::GetSubjectName(cert))
                          .Build())
                  // Subject key information.
                  .Child(
                      CertNodeBuilder(IDS_CERT_DETAILS_SUBJECT_KEY_INFO)
                          .Child(
                              CertNodeBuilder(IDS_CERT_DETAILS_SUBJECT_KEY_ALG)
                                  .Payload(
                                      x509_certificate_model::
                                          ProcessSecAlgorithmSubjectPublicKey(
                                              cert))
                                  .Build())
                          .Child(CertNodeBuilder(IDS_CERT_DETAILS_SUBJECT_KEY)
                                     .Payload(
                                         x509_certificate_model::
                                             ProcessSubjectPublicKeyInfo(cert))
                                     .Build())
                          .Build())
                  // Extensions.
                  .ChildIfNotNull(std::move(details_extensions))
                  .Child(
                      CertNodeBuilder(IDS_CERT_DETAILS_CERTIFICATE_SIG_ALG)
                          .Payload(x509_certificate_model::
                                       ProcessSecAlgorithmSignatureWrap(cert))
                          .Build())
                  .Child(CertNodeBuilder(IDS_CERT_DETAILS_CERTIFICATE_SIG_VALUE)
                             .Payload(x509_certificate_model::
                                          ProcessRawBitsSignatureWrap(cert))
                             .Build())
                  .Child(
                      CertNodeBuilder(IDS_CERT_INFO_FINGERPRINTS_GROUP)
                          .Child(CertNodeBuilder(
                                     IDS_CERT_INFO_SHA256_FINGERPRINT_LABEL)
                                     .Payload(
                                         x509_certificate_model::HashCertSHA256(
                                             cert))
                                     .Build())
                          .Child(
                              CertNodeBuilder(
                                  IDS_CERT_INFO_SHA1_FINGERPRINT_LABEL)
                                  .Payload(x509_certificate_model::HashCertSHA1(
                                      cert))
                                  .Build())
                          .Build())
                  .Build())
          .Build());

  // Send certificate information to javascript.
  web_ui()->CallJavascriptFunctionUnsafe("cert_viewer.getCertificateFields",
                                         root_list);
}

int CertificateViewerDialogHandler::GetCertificateIndex(
    const base::ListValue* args) const {
  int cert_index;
  double val;
  if (!(args->GetDouble(0, &val)))
    return -1;
  cert_index = static_cast<int>(val);
  if (cert_index < 0 || cert_index >= static_cast<int>(cert_chain_.size()))
    return -1;
  return cert_index;
}
