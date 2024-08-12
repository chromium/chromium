// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/certificate_viewer_webui.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/certificate_dialogs.h"
#include "chrome/browser/ui/webui/certificate_viewer_ui.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents.h"
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

namespace {

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
  if (child)
    return Child(std::move(*child));
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
  if (absl::holds_alternative<x509_certificate_model::Error>(s))
    return l10n_util::GetStringUTF8(IDS_CERT_DUMP_ERROR);
  else if (absl::holds_alternative<x509_certificate_model::NotPresent>(s))
    return l10n_util::GetStringUTF8(IDS_CERT_INFO_FIELD_NOT_PRESENT);
  return absl::get<std::string>(s);
}

std::string DialogArgsForCertList(
    const std::vector<x509_certificate_model::X509CertificateModel>& certs) {
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
    if (!children.empty())
      cert_node.Set("children", std::move(children));

    // Add this node to the children list for the next iteration.
    children = base::Value::List();
    children.Append(std::move(cert_node));
    ++index;
  }
  // Set the last node as the top of the certificate hierarchy.
  cert_info.Set("hierarchy", std::move(children));

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
  CertificateViewerDialog* dialog_ptr =
      new CertificateViewerDialog(std::move(certs), std::move(cert_nicknames));
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
    std::vector<std::string> cert_nicknames) {
  CHECK(!in_certs.empty());

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
  set_dialog_args(DialogArgsForCertList(certs));
  set_dialog_modal_type(ui::mojom::ModalType::kNone);
  set_dialog_content_url(GURL(chrome::kChromeUICertificateViewerURL));
  set_dialog_size(kDefaultSize);
  set_dialog_title(l10n_util::GetStringFUTF16(
      IDS_CERT_INFO_DIALOG_TITLE, base::UTF8ToUTF16(certs.front().GetTitle())));
  set_show_dialog_title(true);

  AddWebUIMessageHandler(
      std::make_unique<CertificateViewerDialogHandler>(this, std::move(certs)));
}

CertificateViewerDialog::~CertificateViewerDialog() = default;

////////////////////////////////////////////////////////////////////////////////
// CertificateViewerDialogHandler

CertificateViewerDialogHandler::CertificateViewerDialogHandler(
    CertificateViewerDialog* dialog,
    std::vector<x509_certificate_model::X509CertificateModel> certs)
    : dialog_(dialog), certs_(std::move(certs)) {}

CertificateViewerDialogHandler::~CertificateViewerDialogHandler() {
}

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
}

void CertificateViewerDialogHandler::HandleExportCertificate(
    const base::Value::List& args) {
  int cert_index = GetCertificateIndex(args[0].GetInt());
  if (cert_index < 0)
    return;

  gfx::NativeWindow window =
      platform_util::GetTopLevel(platform_util::GetViewForWindow(
          dialog_->GetNativeWebContentsModalDialog()));

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> export_certs;
  for (const auto& cert : base::make_span(certs_).subspan(cert_index)) {
    export_certs.push_back(bssl::UpRef(cert.cert_buffer()));
  }
  ShowCertExportDialog(web_ui()->GetWebContents(), window,
                       std::move(export_certs), certs_[cert_index].GetTitle());
}

void CertificateViewerDialogHandler::HandleRequestCertificateFields(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  int cert_index = GetCertificateIndex(args[1].GetInt());
  if (cert_index < 0)
    return;

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
