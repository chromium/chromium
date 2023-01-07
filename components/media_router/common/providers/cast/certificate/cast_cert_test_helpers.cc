// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/certificate/cast_cert_test_helpers.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "components/media_router/common/providers/cast/certificate/cast_cert_reader.h"
#include "net/cert/pem.h"
#include "net/cert/pki/cert_errors.h"
#include "net/cert/x509_util.h"

namespace cast_certificate {
namespace testing {

namespace {

base::FilePath GetCastCertificateDirectoryFromPathService() {
  base::FilePath src_root;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root);
  }
  CHECK(!src_root.empty());

  // NOTE: components are appended separately to allow for OS-specific
  // separators to be appended by base::FilePath.
  return src_root.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("media_router")
      .AppendASCII("common")
      .AppendASCII("providers")
      .AppendASCII("cast")
      .AppendASCII("certificate");
}

base::FilePath GetCastCertificatesSubDirectoryFromPathService() {
  return GetCastCertificateDirectory().AppendASCII("certificates");
}

}  // namespace

const base::FilePath& GetCastCertificateDirectory() {
  static const base::NoDestructor<base::FilePath> kPath(
      GetCastCertificateDirectoryFromPathService());
  return *kPath;
}

const base::FilePath& GetCastCertificatesSubDirectory() {
  static const base::NoDestructor<base::FilePath> kPath(
      GetCastCertificatesSubDirectoryFromPathService());
  return *kPath;
}

SignatureTestData ReadSignatureTestData(const base::StringPiece& file_name) {
  SignatureTestData result;

  std::string file_data;
  base::ReadFileToString(GetCastCertificateDirectory().AppendASCII(file_name),
                         &file_data);
  CHECK(!file_data.empty());

  net::PEMTokenizer pem_tokenizer(
      file_data, {"MESSAGE", "SIGNATURE SHA1", "SIGNATURE SHA256"});
  while (pem_tokenizer.GetNext()) {
    const std::string& type = pem_tokenizer.block_type();
    const std::string& value = pem_tokenizer.data();

    if (type == "MESSAGE") {
      result.message = value;
    } else if (type == "SIGNATURE SHA1") {
      result.signature_sha1 = value;
    } else if (type == "SIGNATURE SHA256") {
      result.signature_sha256 = value;
    }
  }

  CHECK(!result.message.empty());
  CHECK(!result.signature_sha1.empty());
  CHECK(!result.signature_sha256.empty());

  return result;
}

base::Time ConvertUnixTimestampSeconds(uint64_t time) {
  return base::Time::UnixEpoch() + base::Seconds(time);
}

std::unique_ptr<net::TrustStoreInMemory> LoadTestCert(
    const base::StringPiece& cert_file_name) {
  auto store = std::make_unique<net::TrustStoreInMemory>();
  CHECK(PopulateStoreWithCertsFromPath(
      store.get(),
      testing::GetCastCertificatesSubDirectory().AppendASCII(cert_file_name)));
  CHECK(store);
  return store;
}
}  // namespace testing
}  // namespace cast_certificate
