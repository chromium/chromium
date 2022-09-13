// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/win/win_platform_delegate.h"

#include <windows.h>

#include <imagehlp.h>
#include <wincrypt.h>

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/process.h"
#include "base/process/process_iterator.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "components/device_signals/core/common/common_types.h"
#include "crypto/scoped_capi_types.h"
#include "crypto/sha2.h"
#include "net/cert/asn1_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device_signals {

namespace {

struct FreeCertBufferFunctor {
  void operator()(WIN_CERTIFICATE* certificate) const {
    if (certificate) {
      delete[] reinterpret_cast<char*>(certificate);
    }
  }
};

// Helper function for expanding all environment variables in `path`.
absl::optional<std::wstring> ExpandEnvironmentVariables(
    const std::wstring& path) {
  static const DWORD kMaxBuffer = 32 * 1024;  // Max according to MSDN.
  std::wstring path_expanded;
  DWORD path_len = MAX_PATH;
  do {
    DWORD result = ::ExpandEnvironmentStrings(
        path.c_str(), base::WriteInto(&path_expanded, path_len), path_len);
    if (!result) {
      // Failed to expand variables.
      break;
    }
    if (result <= path_len)
      return path_expanded.substr(0, result - 1);
    path_len = result;
  } while (path_len < kMaxBuffer);

  return absl::nullopt;
}

}  // namespace

WinPlatformDelegate::WinPlatformDelegate() = default;

WinPlatformDelegate::~WinPlatformDelegate() = default;

bool WinPlatformDelegate::ResolveFilePath(const base::FilePath& file_path,
                                          base::FilePath* resolved_file_path) {
  auto expanded_path_wstring = ExpandEnvironmentVariables(file_path.value());
  if (!expanded_path_wstring) {
    return false;
  }

  auto expanded_file_path = base::FilePath(expanded_path_wstring.value());
  if (!base::PathExists(expanded_file_path)) {
    return false;
  }

  *resolved_file_path = base::MakeAbsoluteFilePath(expanded_file_path);
  return true;
}

absl::optional<std::string>
WinPlatformDelegate::GetSigningCertificatePublicKeyHash(
    const base::FilePath& file_path) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_WIN_SHARE_DELETE);
  if (!file.IsValid()) {
    return absl::nullopt;
  }

  // TODO(b:244573398): Get public key hashes for all certificate indices and
  // return to the extension API as an array.
  ::WIN_CERTIFICATE certificate_header;
  certificate_header.dwLength = 0;
  certificate_header.wRevision = WIN_CERT_REVISION_1_0;
  if (!::ImageGetCertificateHeader(file.GetPlatformFile(),
                                   /*CertificateIndex=*/0,
                                   &certificate_header)) {
    return absl::nullopt;
  }

  DWORD certificate_length = certificate_header.dwLength;
  std::unique_ptr<WIN_CERTIFICATE, FreeCertBufferFunctor> certificate(
      reinterpret_cast<WIN_CERTIFICATE*>(
          new char[sizeof(WIN_CERTIFICATE) + certificate_length]));

  certificate->dwLength = certificate_length;
  certificate->wRevision = WIN_CERT_REVISION_1_0;
  if (!::ImageGetCertificateData(file.GetPlatformFile(), /*CertificateIndex=*/0,
                                 certificate.get(), &certificate_length)) {
    return absl::nullopt;
  }

  PCCERT_CONTEXT raw_certificate_context = nullptr;
  CRYPT_VERIFY_MESSAGE_PARA crypt_verify_param{};
  crypt_verify_param.cbSize = sizeof(crypt_verify_param);
  crypt_verify_param.dwMsgAndCertEncodingType =
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
  if (!::CryptVerifyMessageSignature(
          &crypt_verify_param, /*dwSignerIndex=*/0, certificate->bCertificate,
          certificate->dwLength, NULL, NULL, &raw_certificate_context)) {
    return absl::nullopt;
  }

  crypto::ScopedPCCERT_CONTEXT certificate_context(raw_certificate_context);
  if (!certificate_context || !certificate_context->pbCertEncoded) {
    return absl::nullopt;
  }

  base::StringPiece der_bytes(
      reinterpret_cast<const char*>(certificate_context->pbCertEncoded),
      certificate_context->cbCertEncoded);

  base::StringPiece spki;
  if (!net::asn1::ExtractSPKIFromDERCert(der_bytes, &spki)) {
    return absl::nullopt;
  }

  return crypto::SHA256HashString(spki);
}

}  // namespace device_signals
