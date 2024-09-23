// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/certificate/cast_trust_store.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "components/media_router/common/providers/cast/certificate/cast_cert_reader.h"
#include "components/media_router/common/providers/cast/certificate/switches.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/pki/common_cert_errors.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"

namespace cast_certificate {
namespace {
// Returns a DER-encoded certificate from static storage.
template <size_t N>
std::shared_ptr<const bssl::ParsedCertificate>
ParseCertificateFromStaticStorage(const uint8_t (&data)[N]) {
  bssl::CertErrors errors;
  std::shared_ptr<const bssl::ParsedCertificate> cert =
      bssl::ParsedCertificate::Create(
          net::x509_util::CreateCryptoBufferFromStaticDataUnsafe(data), {},
          &errors);
  CHECK(cert) << errors.ToDebugString();
  return cert;
}

// Returns the cast developer certificate path command line switch if it is
// set. Otherwise, returns an empty file path.
base::FilePath GetDeveloperCertificatePathFromCommandLine() {
  if (!base::CommandLine::InitializedForCurrentProcess()) {
    return base::FilePath();
  }
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->GetSwitchValuePath(
      switches::kCastDeveloperCertificatePath);
}
}  // namespace

void CastTrustStore::AccessInstance(CastTrustStore::AccessCallback callback) {
  CastTrustStore* instance = GetInstance();
  const base::AutoLock guard(instance->lock_);
  std::move(callback).Run(&instance->store_);
}

CastTrustStore* CastTrustStore::GetInstance(bool may_block) {
  static base::NoDestructor<CastTrustStore> instance(may_block);
  return instance.get();
}

CastTrustStore::CastTrustStore(bool may_block) {
  AddDefaultCertificates(may_block);
}

void CastTrustStore::AddDefaultCertificates(bool may_block) {
  AddBuiltInCertificates();

  // Checks to see if there is a developer certificate path was set on the
  // command line. If so, it should be loaded and added to the trust store.
  auto developer_cert_path = GetDeveloperCertificatePathFromCommandLine();
  if (!developer_cert_path.empty()) {
    if (may_block) {
      AddCertificateFromPath(developer_cert_path);
    } else {
      NonBlockingAddCertificateFromPath(developer_cert_path);
    }
  }
}

// -------------------------------------------------------------------------
// Cast trust anchors.
// -------------------------------------------------------------------------

// There are two trusted roots for Cast certificate chains:
//
//   (1) CN=Cast Root CA    (kCastRootCaDer)
//   (2) CN=Eureka Root CA  (kEurekaRootCaDer)
//
// These constants are defined by the files included next:

#include "components/media_router/common/providers/cast/certificate/cast_root_ca_cert_der-inc.h"
#include "components/media_router/common/providers/cast/certificate/eureka_root_ca_der-inc.h"

void CastTrustStore::AddBuiltInCertificates() {
  AddAnchor(ParseCertificateFromStaticStorage(kCastRootCaDer));
  AddAnchor(ParseCertificateFromStaticStorage(kEurekaRootCaDer));
}

void CastTrustStore::NonBlockingAddCertificateFromPath(
    base::FilePath cert_path) {
  // Adding developer certificates must be done off of the IO thread due
  // to blocking file access.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      // NOTE: the singleton instance is never destroyed, so we can use
      // Unretained here instead of a weak pointer.
      base::BindOnce(&CastTrustStore::AddCertificateFromPath,
                     base::Unretained(this), std::move(cert_path)));
}

void CastTrustStore::AddCertificateFromPath(base::FilePath cert_path) {
  if (!cert_path.IsAbsolute()) {
    base::FilePath path;
    base::PathService::Get(base::DIR_CURRENT, &path);
    cert_path = path.Append(cert_path);
  }
  VLOG(1) << "Using cast developer certificate path " << cert_path;
  base::AutoLock guard(lock_);
  if (!PopulateStoreWithCertsFromPath(&store_, cert_path)) {
    LOG(WARNING) << "Unable to add Cast developer certificates at " << cert_path
                 << " to Cast root store; only Google-provided Cast root "
                    "certificates will be used.";
  }
}

void CastTrustStore::ClearCertificatesForTesting() {
  base::AutoLock guard(lock_);
  store_.Clear();
}

void CastTrustStore::AddAnchor(
    std::shared_ptr<const bssl::ParsedCertificate> cert) {
  // Enforce pathlen constraints and policies defined on the root certificate.
  base::AutoLock guard(lock_);
  store_.AddTrustAnchorWithConstraints(std::move(cert));
}

}  // namespace cast_certificate
