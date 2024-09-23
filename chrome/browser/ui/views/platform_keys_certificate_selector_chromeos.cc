// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/platform_keys_certificate_selector_chromeos.h"

#include <stddef.h>
#include <memory>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_private_key.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/style/typography.h"

namespace chromeos {

namespace {

// Fake ClientCertIdentity that does not support retrieving the private key.
// The platformKeys API currently only deals in certificates, not identities.
// Looking up the private key by the certificate is done as a separate step.
class ClientCertIdentityPlatformKeys : public net::ClientCertIdentity {
 public:
  explicit ClientCertIdentityPlatformKeys(
      scoped_refptr<net::X509Certificate> cert)
      : net::ClientCertIdentity(std::move(cert)) {}
  ~ClientCertIdentityPlatformKeys() override = default;

  void AcquirePrivateKey(
      base::OnceCallback<void(scoped_refptr<net::SSLPrivateKey>)>
          private_key_callback) override {
    NOTREACHED();
  }
};

net::ClientCertIdentityList CertificateListToIdentityList(
    const net::CertificateList& certs) {
  net::ClientCertIdentityList identities;
  for (const auto& cert : certs) {
    identities.push_back(
        std::make_unique<ClientCertIdentityPlatformKeys>(cert));
  }
  return identities;
}

}  // namespace

PlatformKeysCertificateSelector::PlatformKeysCertificateSelector(
    const net::CertificateList& certificates,
    const std::string& extension_name,
    CertificateSelectedCallback callback,
    content::WebContents* web_contents)
    : CertificateSelector(CertificateListToIdentityList(certificates),
                          web_contents),
      extension_name_(extension_name),
      callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
  SetCancelCallback(base::BindOnce(
      [](PlatformKeysCertificateSelector* dialog) {
        std::move(dialog->callback_).Run(nullptr);
      },
      base::Unretained(this)));
}

PlatformKeysCertificateSelector::~PlatformKeysCertificateSelector() {
  // Ensure to call back even if the dialog was closed because of the views
  // hierarchy being destroyed.
  if (!callback_.is_null())
    std::move(callback_).Run(nullptr);
}

void PlatformKeysCertificateSelector::Init() {
  const std::u16string name = base::ASCIIToUTF16(extension_name_);

  auto label = std::make_unique<views::StyledLabel>();
  size_t offset;
  label->SetText(l10n_util::GetStringFUTF16(
      IDS_PLATFORM_KEYS_SELECT_CERT_DIALOG_TEXT, name, &offset));

  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.text_style = views::style::STYLE_EMPHASIZED;
  label->AddStyleRange(gfx::Range(offset, offset + name.size()), bold_style);
  CertificateSelector::InitWithText(std::move(label));
}

void PlatformKeysCertificateSelector::AcceptCertificate(
    std::unique_ptr<net::ClientCertIdentity> identity) {
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(base::WrapRefCounted(identity->certificate()));
}

void ShowPlatformKeysCertificateSelector(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const net::CertificateList& certificates,
    CertificateSelectedCallback callback) {
  PlatformKeysCertificateSelector* selector =
      new PlatformKeysCertificateSelector(certificates, extension_name,
                                          std::move(callback), web_contents);
  selector->Init();
  selector->Show();
}

}  // namespace chromeos
