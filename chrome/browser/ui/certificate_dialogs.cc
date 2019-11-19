// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/certificate_dialogs.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "net/cert/x509_util_nss.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

namespace {

enum CertFileType {
  kBase64 = 0,
  kBase64Chain = 1,
  kDer = 2,
  kPkcs7 = 3,
  kPkcs7Chain = 4,
  kNumCertFileTypes = 5,
};

void WriterCallback(const base::FilePath& path, const std::string& data) {
  int bytes_written = base::WriteFile(path, data.data(), data.size());
  if (bytes_written != static_cast<ssize_t>(data.size())) {
    LOG(ERROR) << "Writing " << path.value() << " (" << data.size()
               << "B) returned " << bytes_written;
  }
}

std::string WrapAt64(const std::string& str) {
  std::string result;
  for (size_t i = 0; i < str.size(); i += 64) {
    result.append(str, i, 64);  // Append clamps the len arg internally.
    result.append("\r\n");
  }
  return result;
}

std::string GetBase64String(CERTCertificate* cert) {
  std::string der_cert;
  if (!net::x509_util::GetDEREncoded(cert, &der_cert))
    return std::string();
  std::string base64;
  base::Base64Encode(der_cert, &base64);
  return "-----BEGIN CERTIFICATE-----\r\n" + WrapAt64(base64) +
         "-----END CERTIFICATE-----\r\n";
}

////////////////////////////////////////////////////////////////////////////////
// General utility functions.

class Exporter : public ui::SelectFileDialog::Listener {
 public:
  Exporter(content::WebContents* web_contents,
           gfx::NativeWindow parent,
           net::ScopedCERTCertificateList cert_chain);

  // SelectFileDialog::Listener implemenation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  ~Exporter() override;

  scoped_refptr<ui::SelectFileDialog> const select_file_dialog_;

  // The certificate hierarchy (leaf cert first).
  net::ScopedCERTCertificateList cert_chain_list_;

  DISALLOW_COPY_AND_ASSIGN(Exporter);
};

Exporter::Exporter(content::WebContents* web_contents,
                   gfx::NativeWindow parent,
                   net::ScopedCERTCertificateList cert_chain)
    : select_file_dialog_(ui::SelectFileDialog::Create(
          this,
          std::make_unique<ChromeSelectFilePolicy>(web_contents))),
      cert_chain_list_(std::move(cert_chain)) {
  std::string cert_title =
      x509_certificate_model::GetTitle(cert_chain_list_.begin()->get());
  base::FilePath suggested_name =
      net::GenerateFileName(GURL(),          // url
                            std::string(),   // content_disposition
                            std::string(),   // referrer_charset
                            cert_title,      // suggested_name
                            std::string(),   // mime_type
                            "certificate");  // default_name

  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(web_contents->GetBrowserContext());
  base::FilePath suggested_path =
      download_prefs->SaveFilePath().Append(suggested_name);

  ShowCertSelectFileDialog(select_file_dialog_.get(),
                           ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                           suggested_path, parent, nullptr);
}

Exporter::~Exporter() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_)
    select_file_dialog_->ListenerDestroyed();
}

void Exporter::FileSelected(const base::FilePath& path,
                            int index,
                            void* params) {
  std::string data;
  switch (index - 1) {
    case kBase64Chain:
      for (const auto& cert : cert_chain_list_)
        data += GetBase64String(cert.get());
      break;
    case kDer:
      net::x509_util::GetDEREncoded(cert_chain_list_[0].get(), &data);
      break;
    case kPkcs7:
      data = x509_certificate_model::GetCMSString(cert_chain_list_, 0, 1);
      break;
    case kPkcs7Chain:
      data = x509_certificate_model::GetCMSString(cert_chain_list_, 0,
                                                  cert_chain_list_.size());
      break;
    case kBase64:
    default:
      data = GetBase64String(cert_chain_list_[0].get());
      break;
  }

  if (!data.empty()) {
    base::PostTask(FROM_HERE, {base::ThreadPool(), base::MayBlock()},
                   base::BindOnce(&WriterCallback, path, data));
  }

  delete this;
}

void Exporter::FileSelectionCanceled(void* params) {
  delete this;
}

}  // namespace

void ShowCertSelectFileDialog(ui::SelectFileDialog* select_file_dialog,
                              ui::SelectFileDialog::Type type,
                              const base::FilePath& suggested_path,
                              gfx::NativeWindow parent,
                              void* params) {
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(kNumCertFileTypes);
  file_type_info.extensions[kBase64].push_back(FILE_PATH_LITERAL("pem"));
  file_type_info.extensions[kBase64].push_back(FILE_PATH_LITERAL("crt"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_EXPORT_TYPE_BASE64));
  file_type_info.extensions[kBase64Chain].push_back(FILE_PATH_LITERAL("pem"));
  file_type_info.extensions[kBase64Chain].push_back(FILE_PATH_LITERAL("crt"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_EXPORT_TYPE_BASE64_CHAIN));
  file_type_info.extensions[kDer].push_back(FILE_PATH_LITERAL("der"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_EXPORT_TYPE_DER));
  file_type_info.extensions[kPkcs7].push_back(FILE_PATH_LITERAL("p7c"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_EXPORT_TYPE_PKCS7));
  file_type_info.extensions[kPkcs7Chain].push_back(FILE_PATH_LITERAL("p7c"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_EXPORT_TYPE_PKCS7_CHAIN));
  file_type_info.include_all_files = true;
  select_file_dialog->SelectFile(
      type, base::string16(), suggested_path, &file_type_info,
      1,  // 1-based index for |file_type_info.extensions| to specify default.
      FILE_PATH_LITERAL("crt"), parent, params);
}

void ShowCertExportDialog(content::WebContents* web_contents,
                          gfx::NativeWindow parent,
                          const scoped_refptr<net::X509Certificate>& cert) {
  net::ScopedCERTCertificateList cert_chain =
      net::x509_util::CreateCERTCertificateListFromX509Certificate(cert.get());
  if (cert_chain.empty())
    return;

  // Exporter is self-deleting.
  new Exporter(web_contents, parent, std::move(cert_chain));
}

void ShowCertExportDialog(content::WebContents* web_contents,
                          gfx::NativeWindow parent,
                          net::ScopedCERTCertificateList::iterator certs_begin,
                          net::ScopedCERTCertificateList::iterator certs_end) {
  DCHECK(certs_begin != certs_end);
  net::ScopedCERTCertificateList cert_chain;
  for (auto it = certs_begin; it != certs_end; ++it)
    cert_chain.push_back(net::x509_util::DupCERTCertificate(it->get()));

  // Exporter is self-deleting.
  new Exporter(web_contents, parent, std::move(cert_chain));
}
