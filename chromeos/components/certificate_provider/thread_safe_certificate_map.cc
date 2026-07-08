// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/certificate_provider/thread_safe_certificate_map.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/synchronization/lock.h"
#include "chromeos/components/certificate_provider/certificate_info.h"
#include "net/base/hash_value.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

namespace chromeos {
namespace certificate_provider {
namespace {

std::string GetSubjectPublicKeyInfo(const net::X509Certificate& certificate) {
  std::string_view spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer()),
          &spki_bytes)) {
    return {};
  }
  return std::string(spki_bytes);
}

}  // namespace

ThreadSafeCertificateMap::ThreadSafeCertificateMap() = default;

ThreadSafeCertificateMap::~ThreadSafeCertificateMap() = default;

void ThreadSafeCertificateMap::UpdateCertificatesForExtension(
    const std::string& extension_id,
    const CertificateInfoList& certificates) {
  base::AutoLock auto_lock(lock_);

  base::flat_set<net::SHA256HashValue> current_fingerprints;
  base::flat_set<std::string> current_spkis;
  for (const CertificateInfo& cert_info : certificates) {
    const net::SHA256HashValue fingerprint =
        net::X509Certificate::CalculateFingerprint256(
            cert_info.certificate->cert_buffer());
    current_fingerprints.insert(fingerprint);
    auto [fingerprint_it, fingerprint_inserted] =
        fingerprint_to_extension_and_cert_[fingerprint].try_emplace(
            extension_id);
    if (fingerprint_inserted) {
      fingerprint_it->second.sequence = next_sequence_++;
    }
    fingerprint_it->second.info = cert_info;

    const std::string spki = GetSubjectPublicKeyInfo(*cert_info.certificate);
    current_spkis.insert(spki);
    auto [spki_it, spki_inserted] =
        spki_to_extension_and_cert_[spki].try_emplace(extension_id);
    if (spki_inserted) {
      spki_it->second.sequence = next_sequence_++;
    }
    spki_it->second.info = cert_info;
  }

  // Remove entries for certificates that the extension no longer provides.
  for (auto& [fingerprint, map] : fingerprint_to_extension_and_cert_) {
    if (!current_fingerprints.contains(fingerprint)) {
      map.erase(extension_id);
    }
  }

  base::EraseIf(fingerprint_to_extension_and_cert_,
                [](const auto& entry) { return entry.second.empty(); });

  for (auto& [spki, map] : spki_to_extension_and_cert_) {
    if (!current_spkis.contains(spki)) {
      map.erase(extension_id);
    }
  }

  base::EraseIf(spki_to_extension_and_cert_,
                [](const auto& entry) { return entry.second.empty(); });
}

std::vector<scoped_refptr<net::X509Certificate>>
ThreadSafeCertificateMap::GetCertificates() {
  base::AutoLock auto_lock(lock_);
  std::vector<scoped_refptr<net::X509Certificate>> certificates;
  for (const auto& fingerprint_entry : fingerprint_to_extension_and_cert_) {
    const ExtensionToCertificateMap* extension_to_certificate_map =
        &fingerprint_entry.second;
    if (!extension_to_certificate_map->empty()) {
      // If there are multiple entries with the same fingerprint, they are the
      // same certificate as SHA256 should not have collisions.
      // Since we need each certificate only once, we can return any entry.
      certificates.push_back(
          extension_to_certificate_map->begin()->second.info.certificate);
    }
  }
  return certificates;
}

bool ThreadSafeCertificateMap::LookUpCertificate(
    const net::X509Certificate& cert,
    bool* is_currently_provided,
    CertificateInfo* info,
    std::string* extension_id) {
  *is_currently_provided = false;
  const net::SHA256HashValue fingerprint =
      net::X509Certificate::CalculateFingerprint256(cert.cert_buffer());

  base::AutoLock auto_lock(lock_);
  const auto it = fingerprint_to_extension_and_cert_.find(fingerprint);
  if (it == fingerprint_to_extension_and_cert_.end())
    return false;

  ExtensionToCertificateMap* const map = &it->second;
  if (!map->empty()) {
    const auto map_entry = std::ranges::min_element(
        *map, {}, [](const auto& entry) { return entry.second.sequence; });
    *is_currently_provided = true;
    *info = map_entry->second.info;
    *extension_id = map_entry->first;
  }
  return true;
}

bool ThreadSafeCertificateMap::LookUpCertificateBySpki(
    const std::string& subject_public_key_info,
    bool* is_currently_provided,
    CertificateInfo* info,
    std::string* extension_id) {
  *is_currently_provided = false;
  base::AutoLock auto_lock(lock_);
  const auto it = spki_to_extension_and_cert_.find(subject_public_key_info);
  if (it == spki_to_extension_and_cert_.end())
    return false;

  ExtensionToCertificateMap* const map = &it->second;
  if (!map->empty()) {
    // If multiple extensions provide the same certificate, return the one
    // that started providing it first.
    const auto map_entry = std::ranges::min_element(
        *map, {}, [](const auto& entry) { return entry.second.sequence; });
    *is_currently_provided = true;
    *info = map_entry->second.info;
    *extension_id = map_entry->first;
  }
  return true;
}

void ThreadSafeCertificateMap::RemoveExtension(
    const std::string& extension_id) {
  base::AutoLock auto_lock(lock_);
  RemoveCertificatesProvidedByExtension(extension_id);
}

void ThreadSafeCertificateMap::RemoveCertificatesProvidedByExtension(
    const std::string& extension_id) {
  for (auto& entry : fingerprint_to_extension_and_cert_) {
    ExtensionToCertificateMap* map = &entry.second;
    map->erase(extension_id);
  }

  for (auto& entry : spki_to_extension_and_cert_) {
    ExtensionToCertificateMap* map = &entry.second;
    map->erase(extension_id);
  }
}

}  // namespace certificate_provider
}  // namespace chromeos
