// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/certificate_dialogs.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/pkcs7.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

#if BUILDFLAG(USE_NSS_CERTS)
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "net/cert/x509_util_nss.h"
#endif

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
  if (!base::WriteFile(path, data)) {
    LOG(ERROR) << "Error writing " << path.value() << " (" << data.size()
               << "B)";
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

std::string GetBase64String(const CRYPTO_BUFFER* cert) {
  std::string base64 =
      base::Base64Encode(net::x509_util::CryptoBufferAsSpan(cert));
  return "-----BEGIN CERTIFICATE-----\r\n" + WrapAt64(base64) +
         "-----END CERTIFICATE-----\r\n";
}

////////////////////////////////////////////////////////////////////////////////
// General utility functions.

void ShowCertSelectFileDialogFullExport(
    ui::SelectFileDialog* select_file_dialog,
    const base::FilePath& suggested_path,
    gfx::NativeWindow parent) {
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {
      {FILE_PATH_LITERAL("pem"), FILE_PATH_LITERAL("crt")}};
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_EXPORT_TYPE_BASE64_ALL));
  file_type_info.include_all_files = true;
  select_file_dialog->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
      suggested_path, &file_type_info,
      1,  // 1-based index for |file_type_info.extensions| to specify default.
      FILE_PATH_LITERAL("crt"), parent);
}

class Exporter : public ui::SelectFileDialog::Listener {
 public:
  Exporter(content::WebContents* web_contents,
           gfx::NativeWindow parent,
           std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> cert_chain,
           const std::string& suggested_file_name,
           bool full_export);

  Exporter(const Exporter&) = delete;
  Exporter& operator=(const Exporter&) = delete;

  // SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

 private:
  ~Exporter() override;

  std::string GetCMSString(size_t start, size_t end) const;

  scoped_refptr<ui::SelectFileDialog> const select_file_dialog_;

  // The certificate hierarchy (leaf cert first).
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> cert_chain_list_;

  const bool full_export_;
};

Exporter::Exporter(content::WebContents* web_contents,
                   gfx::NativeWindow parent,
                   std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> cert_chain,
                   const std::string& suggested_file_name,
                   bool full_export)
    : select_file_dialog_(ui::SelectFileDialog::Create(
          this,
          std::make_unique<ChromeSelectFilePolicy>(web_contents))),
      cert_chain_list_(std::move(cert_chain)),
      full_export_(full_export) {
  base::FilePath suggested_name =
      net::GenerateFileName(GURL(),               // url
                            std::string(),        // content_disposition
                            std::string(),        // referrer_charset
                            suggested_file_name,  // suggested_name
                            std::string(),        // mime_type
                            "certificate");       // default_name

  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(web_contents->GetBrowserContext());
  base::FilePath suggested_path =
      download_prefs->SaveFilePath().Append(suggested_name);

  if (full_export_) {
    ShowCertSelectFileDialogFullExport(select_file_dialog_.get(),
                                       suggested_path, parent);
  } else {
    ShowCertSelectFileDialog(select_file_dialog_.get(),
                             ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                             suggested_path, parent);
  }
}

Exporter::~Exporter() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_)
    select_file_dialog_->ListenerDestroyed();
}

void Exporter::FileSelected(const ui::SelectedFileInfo& file, int index) {
  std::string data;

  // If we're doing a full export, the behaviour that is desired is the same as
  // doing the full chain export.
  if (full_export_) {
    index = kBase64Chain + 1;
  }

  switch (index - 1) {
    case kBase64Chain:
      for (const auto& cert : cert_chain_list_)
        data += GetBase64String(cert.get());
      break;
    case kDer:
      data = std::string(
          net::x509_util::CryptoBufferAsStringPiece(cert_chain_list_[0].get()));
      break;
    case kPkcs7:
      data = GetCMSString(0, 1);
      break;
    case kPkcs7Chain:
      data = GetCMSString(0, cert_chain_list_.size());
      break;
    case kBase64:
    default:
      data = GetBase64String(cert_chain_list_[0].get());
      break;
  }

  if (!data.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&WriterCallback, file.path(), data));
  }

  delete this;
}

void Exporter::FileSelectionCanceled() {
  delete this;
}

std::string Exporter::GetCMSString(size_t start, size_t end) const {
  size_t size_hint = 64;
  bssl::UniquePtr<STACK_OF(CRYPTO_BUFFER)> stack(sk_CRYPTO_BUFFER_new_null());
  for (size_t i = start; i < end; ++i) {
    if (!bssl::PushToStack(stack.get(), bssl::UpRef(cert_chain_list_[i])))
      return std::string();
    size_hint += CRYPTO_BUFFER_len(cert_chain_list_[i].get());
  }
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), size_hint) ||
      !PKCS7_bundle_raw_certificates(cbb.get(), stack.get())) {
    return std::string();
  }
  return std::string(reinterpret_cast<const char*>(CBB_data(cbb.get())),
                     CBB_len(cbb.get()));
}

}  // namespace

void ShowCertSelectFileDialog(ui::SelectFileDialog* select_file_dialog,
                              ui::SelectFileDialog::Type type,
                              const base::FilePath& suggested_path,
                              gfx::NativeWindow parent) {
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
      type, std::u16string(), suggested_path, &file_type_info,
      1,  // 1-based index for |file_type_info.extensions| to specify default.
      FILE_PATH_LITERAL("crt"), parent);
}

void ShowCertExportDialog(content::WebContents* web_contents,
                          gfx::NativeWindow parent,
                          std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs,
                          const std::string& cert_title) {
  DCHECK(!certs.empty());
  // Exporter is self-deleting.
  new Exporter(web_contents, parent, std::move(certs), cert_title,
               /*full_export=*/false);
}

#if BUILDFLAG(USE_NSS_CERTS)
void ShowCertExportDialog(content::WebContents* web_contents,
                          gfx::NativeWindow parent,
                          net::ScopedCERTCertificateList::iterator certs_begin,
                          net::ScopedCERTCertificateList::iterator certs_end) {
  DCHECK(certs_begin != certs_end);
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> cert_chain;
  for (auto it = certs_begin; it != certs_end; ++it) {
    cert_chain.push_back(net::x509_util::CreateCryptoBuffer(
        net::x509_util::CERTCertificateAsSpan(it->get())));
  }

  // Exporter is self-deleting.
  new Exporter(web_contents, parent, std::move(cert_chain),
               x509_certificate_model::GetTitle(certs_begin->get()),
               /*full_export=*/false);
}
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
void ShowCertExportDialogSaveAll(
    content::WebContents* web_contents,
    gfx::NativeWindow parent,
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs,
    const std::string& suggested_file_name) {
  DCHECK(!certs.empty());
  // Exporter is self-deleting.
  new Exporter(web_contents, parent, std::move(certs), suggested_file_name,
               /*full_export=*/true);
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
