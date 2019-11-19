// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/module_info_util.h"

#include <windows.h>

#include <tlhelp32.h>
#include <wintrust.h>

#include <limits>
#include <memory>
#include <string>

#include "base/environment.h"
#include "base/files/file.h"
#include "base/i18n/case_conversion.h"
#include "base/scoped_generic.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/wincrypt_shim.h"
#include "chrome/common/safe_browsing/pe_image_reader_win.h"

// This must be after wincrypt and wintrust.
#include <mscat.h>

namespace {

// Helper for scoped tracking an HCERTSTORE.
struct ScopedHCERTSTORETraits {
  static HCERTSTORE InvalidValue() { return nullptr; }
  static void Free(HCERTSTORE store) { ::CertCloseStore(store, 0); }
};
using ScopedHCERTSTORE =
    base::ScopedGeneric<HCERTSTORE, ScopedHCERTSTORETraits>;

// Helper for scoped tracking an HCRYPTMSG.
struct ScopedHCRYPTMSGTraits {
  static HCRYPTMSG InvalidValue() { return nullptr; }
  static void Free(HCRYPTMSG message) { ::CryptMsgClose(message); }
};
using ScopedHCRYPTMSG = base::ScopedGeneric<HCRYPTMSG, ScopedHCRYPTMSGTraits>;

// Returns the "Subject" field from the digital signature in the provided
// binary, if any is present. Returns an empty string on failure.
base::string16 GetSubjectNameInFile(const base::FilePath& filename) {
  ScopedHCERTSTORE store;
  ScopedHCRYPTMSG message;

  // Find the crypto message for this filename.
  {
    HCERTSTORE temp_store = nullptr;
    HCRYPTMSG temp_message = nullptr;
    bool result =
        !!CryptQueryObject(CERT_QUERY_OBJECT_FILE, filename.value().c_str(),
                           CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                           CERT_QUERY_FORMAT_FLAG_BINARY, 0, nullptr, nullptr,
                           nullptr, &temp_store, &temp_message, nullptr);
    store.reset(temp_store);
    message.reset(temp_message);
    if (!result)
      return base::string16();
  }

  // Determine the size of the signer info data.
  DWORD signer_info_size = 0;
  bool result = !!CryptMsgGetParam(message.get(), CMSG_SIGNER_INFO_PARAM, 0,
                                   nullptr, &signer_info_size);
  if (!result)
    return base::string16();

  // Allocate enough space to hold the signer info.
  std::unique_ptr<BYTE[]> signer_info_buffer(new BYTE[signer_info_size]);
  CMSG_SIGNER_INFO* signer_info =
      reinterpret_cast<CMSG_SIGNER_INFO*>(signer_info_buffer.get());

  // Obtain the signer info.
  result = !!CryptMsgGetParam(message.get(), CMSG_SIGNER_INFO_PARAM, 0,
                              signer_info, &signer_info_size);
  if (!result)
    return base::string16();

  // Search for the signer certificate.
  CERT_INFO CertInfo = {0};
  PCCERT_CONTEXT cert_context = nullptr;
  CertInfo.Issuer = signer_info->Issuer;
  CertInfo.SerialNumber = signer_info->SerialNumber;

  cert_context = CertFindCertificateInStore(
      store.get(), X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
      CERT_FIND_SUBJECT_CERT, &CertInfo, nullptr);
  if (!cert_context)
    return base::string16();

  // Determine the size of the Subject name.
  DWORD subject_name_size = CertGetNameString(
      cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr, 0);
  if (!subject_name_size)
    return base::string16();

  base::string16 subject_name;
  subject_name.resize(subject_name_size);

  // Get subject name.
  if (!(CertGetNameString(cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0,
                          nullptr, const_cast<LPWSTR>(subject_name.c_str()),
                          subject_name_size))) {
    return base::string16();
  }

  // The subject name is normalized because it can contain trailing null
  // characters.
  internal::NormalizeCertificateSubject(&subject_name);

  return subject_name;
}

// Helper for scoped tracking a catalog admin context.
struct CryptCATContextScopedTraits {
  static PVOID InvalidValue() { return nullptr; }
  static void Free(PVOID context) { CryptCATAdminReleaseContext(context, 0); }
};
using ScopedCryptCATContext =
    base::ScopedGeneric<PVOID, CryptCATContextScopedTraits>;

// Helper for scoped tracking of a catalog context. A catalog context is only
// valid with an associated admin context, so this is effectively a std::pair.
// A custom operator!= is required in order for a null |catalog_context| but
// non-null |context| to compare equal to the InvalidValue exposed by the
// traits class.
class CryptCATCatalogContext {
 public:
  CryptCATCatalogContext(PVOID context, PVOID catalog_context)
      : context_(context), catalog_context_(catalog_context) {}

  bool operator!=(const CryptCATCatalogContext& rhs) const {
    return catalog_context_ != rhs.catalog_context_;
  }

  PVOID context() const { return context_; }
  PVOID catalog_context() const { return catalog_context_; }

 private:
  PVOID context_;
  PVOID catalog_context_;
};

struct CryptCATCatalogContextScopedTraits {
  static CryptCATCatalogContext InvalidValue() {
    return CryptCATCatalogContext(nullptr, nullptr);
  }
  static void Free(const CryptCATCatalogContext& c) {
    CryptCATAdminReleaseCatalogContext(c.context(), c.catalog_context(), 0);
  }
};
using ScopedCryptCATCatalogContext =
    base::ScopedGeneric<CryptCATCatalogContext,
                        CryptCATCatalogContextScopedTraits>;

// Extracts the subject name and catalog path if the provided file is present in
// a catalog file.
void GetCatalogCertificateInfo(const base::FilePath& filename,
                               CertificateInfo* certificate_info) {
  // Get a crypt context for signature verification.
  ScopedCryptCATContext context;
  {
    PVOID raw_context = nullptr;
    if (!CryptCATAdminAcquireContext(&raw_context, nullptr, 0))
      return;
    context.reset(raw_context);
  }

  // Open the file of interest.
  base::win::ScopedHandle file_handle(
      CreateFileW(filename.value().c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, 0, nullptr));
  if (!file_handle.IsValid())
    return;

  // Get the size we need for our hash.
  DWORD hash_size = 0;
  CryptCATAdminCalcHashFromFileHandle(file_handle.Get(), &hash_size, nullptr,
                                      0);
  if (hash_size == 0)
    return;

  // Calculate the hash. If this fails then bail.
  std::vector<BYTE> buffer(hash_size);
  if (!CryptCATAdminCalcHashFromFileHandle(file_handle.Get(), &hash_size,
                                           buffer.data(), 0)) {
    return;
  }

  // Get catalog for our context.
  ScopedCryptCATCatalogContext catalog_context(CryptCATCatalogContext(
      context.get(), CryptCATAdminEnumCatalogFromHash(
                         context.get(), buffer.data(), hash_size, 0, nullptr)));
  if (!catalog_context.is_valid())
    return;

  // Get the catalog info. This includes the path to the catalog itself, which
  // contains the signature of interest.
  CATALOG_INFO catalog_info = {};
  catalog_info.cbStruct = sizeof(catalog_info);
  if (!CryptCATCatalogInfoFromContext(catalog_context.get().catalog_context(),
                                      &catalog_info, 0)) {
    return;
  }

  // Attempt to get the "Subject" field from the signature of the catalog file
  // itself.
  base::FilePath catalog_path(catalog_info.wszCatalogFile);
  base::string16 subject = GetSubjectNameInFile(catalog_path);

  if (subject.empty())
    return;

  certificate_info->type = CertificateInfo::Type::CERTIFICATE_IN_CATALOG;
  certificate_info->path = catalog_path;
  certificate_info->subject = subject;
}

}  // namespace

const wchar_t kClassIdRegistryKeyFormat[] = L"CLSID\\%ls\\InProcServer32";

// ModuleDatabase::CertificateInfo ---------------------------------------------

CertificateInfo::CertificateInfo() : type(Type::NO_CERTIFICATE) {}

// Extracts information about the certificate of the given file, if any is
// found.
void GetCertificateInfo(const base::FilePath& filename,
                        CertificateInfo* certificate_info) {
  DCHECK_EQ(CertificateInfo::Type::NO_CERTIFICATE, certificate_info->type);
  DCHECK(certificate_info->path.empty());
  DCHECK(certificate_info->subject.empty());

  GetCatalogCertificateInfo(filename, certificate_info);
  if (certificate_info->type == CertificateInfo::Type::CERTIFICATE_IN_CATALOG)
    return;

  base::string16 subject = GetSubjectNameInFile(filename);
  if (subject.empty())
    return;

  certificate_info->type = CertificateInfo::Type::CERTIFICATE_IN_FILE;
  certificate_info->path = filename;
  certificate_info->subject = subject;
}

bool IsMicrosoftModule(base::StringPiece16 subject) {
  static constexpr wchar_t kMicrosoft[] = L"Microsoft ";
  return subject.starts_with(kMicrosoft);
}

StringMapping GetEnvironmentVariablesMapping(
    const std::vector<base::string16>& environment_variables) {
  std::unique_ptr<base::Environment> environment(base::Environment::Create());

  StringMapping string_mapping;
  for (const base::string16& variable : environment_variables) {
    std::string value;
    if (environment->GetVar(base::UTF16ToASCII(variable).c_str(), &value)) {
      value = base::TrimString(value, "\\", base::TRIM_TRAILING).as_string();
      string_mapping.push_back(
          std::make_pair(base::i18n::ToLower(base::UTF8ToUTF16(value)),
                         L"%" + base::i18n::ToLower(variable) + L"%"));
    }
  }

  return string_mapping;
}

void CollapseMatchingPrefixInPath(const StringMapping& prefix_mapping,
                                  base::string16* path) {
  const base::string16 path_copy = *path;
  DCHECK_EQ(base::i18n::ToLower(path_copy), path_copy);

  size_t min_length = std::numeric_limits<size_t>::max();
  for (const auto& mapping : prefix_mapping) {
    DCHECK_EQ(base::i18n::ToLower(mapping.first), mapping.first);
    if (base::StartsWith(path_copy, mapping.first,
                         base::CompareCase::SENSITIVE)) {
      // Make sure the matching prefix is a full path component.
      if (path_copy[mapping.first.length()] != '\\' &&
          path_copy[mapping.first.length()] != '\0') {
        continue;
      }

      base::string16 collapsed_path = path_copy;
      base::ReplaceFirstSubstringAfterOffset(&collapsed_path, 0, mapping.first,
                                             mapping.second);
      size_t length = collapsed_path.length() - mapping.second.length();
      if (length < min_length) {
        *path = collapsed_path;
        min_length = length;
      }
    }
  }
}

bool GetModuleImageSizeAndTimeDateStamp(const base::FilePath& path,
                                        uint32_t* size_of_image,
                                        uint32_t* time_date_stamp) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return false;

  // The values fetched here from the NT header live in the first 4k bytes of
  // the file in a well-formed dll.
  constexpr size_t kPageSize = 4096;

  // Note: std::make_unique() is explicitly avoided because it does value-
  //       initialization on arrays, which is not needed in this case.
  auto buffer = std::unique_ptr<uint8_t[]>(new uint8_t[kPageSize]);
  int bytes_read =
      file.Read(0, reinterpret_cast<char*>(buffer.get()), kPageSize);
  if (bytes_read == -1)
    return false;

  safe_browsing::PeImageReader pe_image_reader;
  if (!pe_image_reader.Initialize(buffer.get(), bytes_read))
    return false;

  *size_of_image = pe_image_reader.GetSizeOfImage();
  *time_date_stamp = pe_image_reader.GetCoffFileHeader()->TimeDateStamp;

  return true;
}

namespace internal {

void NormalizeCertificateSubject(base::string16* subject) {
  size_t first_null = subject->find(L'\0');
  if (first_null != base::string16::npos)
    subject->resize(first_null);
}

}  // namespace internal
