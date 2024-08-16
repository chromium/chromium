// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/updater/certificate_tag.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "chrome/updater/certificate_tag_internal.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"

namespace updater::tagging {

namespace internal {

CBS CBSFromSpan(base::span<const uint8_t> span) {
  CBS cbs;
  CBS_init(&cbs, span.data(), span.size());
  return cbs;
}

base::span<const uint8_t> SpanFromCBS(const CBS* cbs) {
  return base::span<const uint8_t>(CBS_data(cbs), CBS_len(cbs));
}

PEBinary::PEBinary(const PEBinary&) = default;
PEBinary::~PEBinary() = default;

// static
std::unique_ptr<PEBinary> PEBinary::Parse(base::span<const uint8_t> binary) {
  // Parse establishes some offsets into |binary| for structures that |GetTag|
  // and |SetTag| will both need.

  // kPEHeaderOffsetOffset is the offset into the binary where the offset of the
  // PE header is found.
  static constexpr size_t kPEHeaderOffsetOffset = 0x3c;
  static constexpr uint32_t kPEMagic = 0x4550;  // "PE\x00\x00"

  // These are a subset of the known COFF "characteristic" flags.
  static constexpr uint16_t kCOFFCharacteristicExecutableImage = 2;
  static constexpr uint16_t kCOFFCharacteristicDLL = 0x2000;

  static constexpr int kPE32Magic = 0x10b;
  static constexpr int kPE32PlusMagic = 0x20b;
  static constexpr size_t kCertificateTableIndex = 4;
  static constexpr size_t kFileHeaderSize = 20;

  CBS bin = CBSFromSpan(binary);
  CBS bin_for_offset = bin;
  CBS bin_for_header = bin;
  uint32_t pe_offset = 0, pe_magic = 0;
  uint16_t size_of_optional_header = 0, characteristics = 0,
           optional_header_magic = 0;
  CBS optional_header;
  // See the IMAGE_FILE_HEADER structure from
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms680313(v=vs.85).aspx.
  if (!CBS_skip(&bin_for_offset, kPEHeaderOffsetOffset) ||
      !CBS_get_u32le(&bin_for_offset, &pe_offset) ||
      !CBS_skip(&bin_for_header, pe_offset) ||
      !CBS_get_u32le(&bin_for_header, &pe_magic) || pe_magic != kPEMagic ||
      // http://msdn.microsoft.com/en-us/library/windows/desktop/ms680313(v=vs.85).aspx
      !CBS_skip(&bin_for_header, 16) ||
      !CBS_get_u16le(&bin_for_header, &size_of_optional_header) ||
      !CBS_get_u16le(&bin_for_header, &characteristics) ||
      (characteristics & kCOFFCharacteristicExecutableImage) == 0 ||
      (characteristics & kCOFFCharacteristicDLL) != 0 ||
      // See the IMAGE_OPTIONAL_HEADER structure from
      // http://msdn.microsoft.com/en-us/library/windows/desktop/ms680339(v=vs.85).aspx.
      !CBS_get_bytes(&bin_for_header, &optional_header,
                     size_of_optional_header) ||
      !CBS_get_u16le(&optional_header, &optional_header_magic)) {
    return {};
  }

  size_t address_size = 0, extra_header_bytes = 0;
  switch (optional_header_magic) {
    case kPE32PlusMagic:
      address_size = 8;
      break;
    case kPE32Magic:
      address_size = 4;
      // PE32 contains an additional field in the optional header.
      extra_header_bytes = 4;
      break;
    default:
      return {};
  }

  // Skip the Windows-specific header section up until the number of data
  // directory entries.
  const size_t to_skip =
      22 + extra_header_bytes + address_size + 40 + address_size * 4 + 4;
  uint32_t num_directory_entries = 0, cert_entry_virtual_addr = 0,
           cert_entry_size = 0;
  if (!CBS_skip(&optional_header, to_skip) ||
      // Read the number of directory entries, which is also the last value
      // in the Windows-specific header.
      !CBS_get_u32le(&optional_header, &num_directory_entries) ||
      num_directory_entries > 4096 ||
      num_directory_entries <= kCertificateTableIndex ||
      !CBS_skip(&optional_header, kCertificateTableIndex * 8) ||
      // See the IMAGE_DATA_DIRECTORY structure from
      // http://msdn.microsoft.com/en-us/library/windows/desktop/ms680305(v=vs.85).aspx.
      !CBS_get_u32le(&optional_header, &cert_entry_virtual_addr) ||
      !CBS_get_u32le(&optional_header, &cert_entry_size) ||
      size_t{cert_entry_virtual_addr} + cert_entry_size < cert_entry_size ||
      size_t{cert_entry_virtual_addr} + cert_entry_size != CBS_len(&bin)) {
    return {};
  }

  CBS bin_for_certs = bin;
  CBS certs;
  if (!CBS_skip(&bin_for_certs, cert_entry_virtual_addr) ||
      !CBS_get_bytes(&bin_for_certs, &certs, cert_entry_size)) {
    return {};
  }

  // See the WIN_CERTIFICATE structure from
  // http://msdn.microsoft.com/en-us/library/ms920091.aspx.
  uint32_t certs_len = 0;
  uint16_t revision = 0, certs_type = 0;
  CBS signed_data;
  const size_t expected_certs_len = CBS_len(&certs);
  if (!CBS_get_u32le(&certs, &certs_len) || certs_len != expected_certs_len ||
      !CBS_get_u16le(&certs, &revision) ||
      revision != kAttributeCertificateRevision ||
      !CBS_get_u16le(&certs, &certs_type) ||
      certs_type != kAttributeCertificateTypePKCS7SignedData ||
      !CBS_get_asn1_element(&certs, &signed_data, CBS_ASN1_SEQUENCE)) {
    return {};
  }

  auto ret = std::make_unique<PEBinary>();
  ret->certs_size_offset_ =
      pe_offset + 4 + kFileHeaderSize + size_of_optional_header -
      8 * (num_directory_entries - kCertificateTableIndex) + 4;

  // Double-check that the calculated |certs_size_offset_| is correct by reading
  // from that location and checking that the value is as expected.
  uint32_t cert_entry_size_duplicate = 0;
  CBS bin_for_check = bin;
  if (!CBS_skip(&bin_for_check, ret->certs_size_offset_) ||
      !CBS_get_u32le(&bin_for_check, &cert_entry_size_duplicate) ||
      cert_entry_size_duplicate != cert_entry_size) {
    NOTREACHED_IN_MIGRATION();
    return {};
  }

  ret->binary_ = binary;
  ret->content_info_ = SpanFromCBS(&signed_data);
  ret->attr_cert_offset_ = cert_entry_virtual_addr;

  if (!ret->ParseTag()) {
    return {};
  }

  return ret;
}

std::optional<std::vector<uint8_t>> PEBinary::tag() const {
  return tag_;
}

bool AddName(CBB* cbb, const char* common_name) {
  // kCommonName is the DER-enabled OID for common names.
  static constexpr uint8_t kCommonName[] = {0x55, 0x04, 0x03};

  CBB name, rdns, rdn, oid, str;
  if (!CBB_add_asn1(cbb, &name, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&name, &rdns, CBS_ASN1_SET) ||
      !CBB_add_asn1(&rdns, &rdn, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&rdn, &oid, CBS_ASN1_OBJECT) ||
      !CBB_add_bytes(&oid, kCommonName, sizeof(kCommonName)) ||
      !CBB_add_asn1(&rdn, &str, CBS_ASN1_UTF8STRING) ||
      !CBB_add_bytes(&str, reinterpret_cast<const uint8_t*>(common_name),
                     strlen(common_name)) ||
      !CBB_flush(cbb)) {
    return false;
  }

  return true;
}

bool CopyASN1(CBB* out, CBS* in) {
  CBS element;
  return CBS_get_any_asn1_element(in, &element, nullptr, nullptr) == 1 &&
         CBB_add_bytes(out, CBS_data(&element), CBS_len(&element)) == 1;
}

ParseResult ParseTagImpl(base::span<const uint8_t> signed_data) {
  CBS content_info = CBSFromSpan(signed_data);
  CBS pkcs7, certs;
  // See https://tools.ietf.org/html/rfc2315#section-7
  if (!CBS_get_asn1(&content_info, &content_info, CBS_ASN1_SEQUENCE) ||
      // type
      !CBS_get_asn1(&content_info, nullptr, CBS_ASN1_OBJECT) ||
      !CBS_get_asn1(&content_info, &pkcs7,
                    0 | CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC) ||
      // See https://tools.ietf.org/html/rfc2315#section-9.1
      !CBS_get_asn1(&pkcs7, &pkcs7, CBS_ASN1_SEQUENCE) ||
      // version
      !CBS_get_asn1(&pkcs7, nullptr, CBS_ASN1_INTEGER) ||
      // digests
      !CBS_get_asn1(&pkcs7, nullptr, CBS_ASN1_SET) ||
      // contentInfo
      !CBS_get_asn1(&pkcs7, nullptr, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&pkcs7, &certs,
                    0 | CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC)) {
    return {};
  }

  bool have_last_cert = false;
  CBS last_cert;

  while (CBS_len(&certs) > 0) {
    if (!CBS_get_asn1(&certs, &last_cert, CBS_ASN1_SEQUENCE)) {
      return {};
    }
    have_last_cert = true;
  }

  if (!have_last_cert) {
    return {};
  }

  // See https://tools.ietf.org/html/rfc5280#section-4.1 for the X.509 structure
  // being parsed here.
  CBS tbs_cert, outer_extensions;
  int has_extensions = 0;
  if (!CBS_get_asn1(&last_cert, &tbs_cert, CBS_ASN1_SEQUENCE) ||
      // version
      !CBS_get_optional_asn1(
          &tbs_cert, nullptr, nullptr,
          CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 0) ||
      // serialNumber
      !CBS_get_asn1(&tbs_cert, nullptr, CBS_ASN1_INTEGER) ||
      // signature algorithm
      !CBS_get_asn1(&tbs_cert, nullptr, CBS_ASN1_SEQUENCE) ||
      // issuer
      !CBS_get_asn1(&tbs_cert, nullptr, CBS_ASN1_SEQUENCE) ||
      // validity
      !CBS_get_asn1(&tbs_cert, nullptr, CBS_ASN1_SEQUENCE) ||
      // subject
      !CBS_get_asn1(&tbs_cert, nullptr, CBS_ASN1_SEQUENCE) ||
      // subjectPublicKeyInfo
      !CBS_get_asn1(&tbs_cert, nullptr, CBS_ASN1_SEQUENCE) ||
      // issuerUniqueID
      !CBS_get_optional_asn1(&tbs_cert, nullptr, nullptr,
                             CBS_ASN1_CONTEXT_SPECIFIC | 1) ||
      // subjectUniqueID
      !CBS_get_optional_asn1(&tbs_cert, nullptr, nullptr,
                             CBS_ASN1_CONTEXT_SPECIFIC | 2) ||
      !CBS_get_optional_asn1(
          &tbs_cert, &outer_extensions, &has_extensions,
          CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 3)) {
    return {};
  }

  if (!has_extensions) {
    return {};
  }

  CBS extensions;
  if (!CBS_get_asn1(&outer_extensions, &extensions, CBS_ASN1_SEQUENCE)) {
    return {};
  }

  while (CBS_len(&extensions) > 0) {
    CBS extension, oid, contents;
    if (!CBS_get_asn1(&extensions, &extension, CBS_ASN1_SEQUENCE) ||
        !CBS_get_asn1(&extension, &oid, CBS_ASN1_OBJECT) ||
        (CBS_peek_asn1_tag(&extension, CBS_ASN1_BOOLEAN) &&
         !CBS_get_asn1(&extension, nullptr, CBS_ASN1_BOOLEAN)) ||
        !CBS_get_asn1(&extension, &contents, CBS_ASN1_OCTETSTRING) ||
        CBS_len(&extension) != 0) {
      return {};
    }

    if (CBS_len(&oid) == sizeof(kTagOID) &&
        memcmp(CBS_data(&oid), kTagOID, sizeof(kTagOID)) == 0) {
      return {true, SpanFromCBS(&contents)};
    }
  }

  return {true, std::nullopt};
}

std::optional<std::vector<uint8_t>> SetTagImpl(
    base::span<const uint8_t> signed_data,
    base::span<const uint8_t> tag) {
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), signed_data.size() + 1024)) {
    return std::nullopt;
  }

  // Walk the PKCS SignedData structure from the input and copy elements to the
  // output until the list of certificates is reached.
  CBS content_info = CBSFromSpan(signed_data);
  CBS pkcs7, certs;
  CBB content_info_cbb, outer_pkcs7_cbb, pkcs7_cbb, certs_cbb;
  if (!CBS_get_asn1(&content_info, &content_info, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(cbb.get(), &content_info_cbb, CBS_ASN1_SEQUENCE) ||
      // See https://tools.ietf.org/html/rfc2315#section-7
      // type
      !CopyASN1(&content_info_cbb, &content_info) ||
      !CBS_get_asn1(&content_info, &pkcs7,
                    0 | CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC) ||
      !CBB_add_asn1(&content_info_cbb, &outer_pkcs7_cbb,
                    0 | CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC) ||
      // See https://tools.ietf.org/html/rfc2315#section-9.1
      !CBS_get_asn1(&pkcs7, &pkcs7, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&outer_pkcs7_cbb, &pkcs7_cbb, CBS_ASN1_SEQUENCE) ||
      // version
      !CopyASN1(&pkcs7_cbb, &pkcs7) ||
      // digests
      !CopyASN1(&pkcs7_cbb, &pkcs7) ||
      // contentInfo
      !CopyASN1(&pkcs7_cbb, &pkcs7) ||
      !CBS_get_asn1(&pkcs7, &certs,
                    0 | CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC) ||
      !CBB_add_asn1(&pkcs7_cbb, &certs_cbb,
                    0 | CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC)) {
    return std::nullopt;
  }

  // Copy the certificates from the input to the output, potentially omitting
  // the last one if it's a superfluous cert.
  bool have_last_cert = false;
  CBS last_cert;

  while (CBS_len(&certs) > 0) {
    if ((have_last_cert && !CBB_add_bytes(&certs_cbb, CBS_data(&last_cert),
                                          CBS_len(&last_cert))) ||
        !CBS_get_asn1_element(&certs, &last_cert, CBS_ASN1_SEQUENCE)) {
      return std::nullopt;
    }
    have_last_cert = true;
  }

  if (!have_last_cert) {
    return std::nullopt;
  }

  // If there's not already a tag then we need to keep the last certificate.
  // Otherwise it's the certificate with the tag in and we're going to replace
  // it.
  const ParseResult result = ParseTagImpl(signed_data);
  if (!result.tag &&
      !CBB_add_bytes(&certs_cbb, CBS_data(&last_cert), CBS_len(&last_cert))) {
    return std::nullopt;
  }

  // These values are DER-encoded OIDs needed in the X.509 certificate that's
  // constructed below.
  static constexpr uint8_t kSHA256WithRSA[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                               0x0d, 0x01, 0x01, 0x0b};
  static constexpr uint8_t kECPublicKey[] = {0x2a, 0x86, 0x48, 0xce,
                                             0x3d, 0x02, 0x01};
  static constexpr uint8_t kP256[] = {
      0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
  };
  static constexpr uint8_t kSHA256RSAEncryption[] = {
      0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
  };
  // kPublicKeyPoint is a X9.62, uncompressed P-256 point where x = 0.
  static constexpr uint8_t kPublicKeyPoint[] = {
      0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x66, 0x48, 0x5c, 0x78, 0x0e, 0x2f, 0x83, 0xd7, 0x24, 0x33, 0xbd,
      0x5d, 0x84, 0xa0, 0x6b, 0xb6, 0x54, 0x1c, 0x2a, 0xf3, 0x1d, 0xae,
      0x87, 0x17, 0x28, 0xbf, 0x85, 0x6a, 0x17, 0x4f, 0x93, 0xf4,
  };

  // Create a mock certificate with an extension containing the tag. See
  // https://tools.ietf.org/html/rfc5280#section-4.1 for the structure that's
  // being created here.
  CBB cert, tbs_cert, version, spki, sigalg, sigalg_oid, validity, not_before,
      not_after, key_params, public_key, extensions_tag, extensions, extension,
      critical_flag, tag_cbb, oid, null, signature;
  uint8_t* cbb_data = nullptr;
  size_t cbb_len = 0;
  if (!CBB_add_asn1(&certs_cbb, &cert, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&cert, &tbs_cert, CBS_ASN1_SEQUENCE) ||
      // version
      !CBB_add_asn1(&tbs_cert, &version,
                    CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC | 0) ||
      !CBB_add_asn1_uint64(&version,
                           2 /* version 3, because X.509 is off by one. */) ||
      // serialNumber
      !CBB_add_asn1_uint64(&tbs_cert, 1) ||
      // signature algorithm
      !CBB_add_asn1(&tbs_cert, &sigalg, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&sigalg, &sigalg_oid, CBS_ASN1_OBJECT) ||
      !CBB_add_bytes(&sigalg_oid, kSHA256WithRSA, sizeof(kSHA256WithRSA)) ||
      !CBB_add_asn1(&sigalg, &null, CBS_ASN1_NULL) ||
      // issuer
      !AddName(&tbs_cert, "Dummy issuer") ||
      !CBB_add_asn1(&tbs_cert, &validity, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&validity, &not_before, CBS_ASN1_UTCTIME) ||
      !CBB_add_bytes(&not_before,
                     reinterpret_cast<const uint8_t*>("130101100000Z"), 13) ||
      !CBB_add_asn1(&validity, &not_after, CBS_ASN1_UTCTIME) ||
      !CBB_add_bytes(&not_after,
                     reinterpret_cast<const uint8_t*>("130401100000Z"), 13) ||
      // subject
      !AddName(&tbs_cert, "Dummy certificate") ||
      // subjectPublicKeyInfo
      !CBB_add_asn1(&tbs_cert, &spki, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&spki, &key_params, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&key_params, &oid, CBS_ASN1_OBJECT) ||
      !CBB_add_bytes(&oid, kECPublicKey, sizeof(kECPublicKey)) ||
      !CBB_add_asn1(&key_params, &oid, CBS_ASN1_OBJECT) ||
      !CBB_add_bytes(&oid, kP256, sizeof(kP256)) ||
      !CBB_add_asn1(&spki, &public_key, CBS_ASN1_BITSTRING) ||
      // Zero unused bits in BITSTRING.
      !CBB_add_bytes(&public_key, reinterpret_cast<const uint8_t*>(""), 1) ||
      !CBB_add_bytes(&public_key, kPublicKeyPoint, sizeof(kPublicKeyPoint)) ||
      !CBB_add_asn1(&tbs_cert, &extensions_tag,
                    3 | CBS_ASN1_CONSTRUCTED | CBS_ASN1_CONTEXT_SPECIFIC) ||
      !CBB_add_asn1(&extensions_tag, &extensions, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&extensions, &extension, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&extension, &oid, CBS_ASN1_OBJECT) ||
      !CBB_add_bytes(&oid, kTagOID, sizeof(kTagOID)) ||
      !CBB_add_asn1(&extension, &critical_flag, CBS_ASN1_BOOLEAN) ||
      // Not critical.
      !CBB_add_bytes(&critical_flag, reinterpret_cast<const uint8_t*>(""), 1) ||
      !CBB_add_asn1(&extension, &tag_cbb, CBS_ASN1_OCTETSTRING) ||
      !CBB_add_bytes(&tag_cbb, tag.data(), tag.size()) ||
      !CBB_add_asn1(&cert, &sigalg, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&sigalg, &sigalg_oid, CBS_ASN1_OBJECT) ||
      !CBB_add_bytes(&sigalg_oid, kSHA256RSAEncryption,
                     sizeof(kSHA256RSAEncryption)) ||
      !CBB_add_asn1(&sigalg, &null, CBS_ASN1_NULL) ||
      !CBB_add_asn1(&cert, &signature, CBS_ASN1_BITSTRING) ||
      // Dummy, 1-byte signature.
      !CBB_add_bytes(&signature, reinterpret_cast<const uint8_t*>("\x00"), 2) ||
      // Copy signerInfos from the input PKCS#7 structure.
      !CopyASN1(&pkcs7_cbb, &pkcs7) || CBS_len(&pkcs7) != 0 ||
      !CBB_finish(cbb.get(), &cbb_data, &cbb_len)) {
    return std::nullopt;
  }

  // Copy the CBB result into a std::vector, padding to 8-byte alignment.
  std::vector<uint8_t> ret;
  const size_t padding = (8 - cbb_len % 8) % 8;
  ret.reserve(cbb_len + padding);
  ret.insert(ret.begin(), cbb_data, cbb_data + cbb_len);
  ret.insert(ret.end(), padding, 0);
  OPENSSL_free(cbb_data);

  return ret;
}

std::optional<std::vector<uint8_t>> PEBinary::SetTag(
    base::span<const uint8_t> tag) {
  std::optional<std::vector<uint8_t>> ret = SetTagImpl(content_info_, tag);
  if (!ret) {
    return std::nullopt;
  }

  // Recreate the header for the `WIN_CERTIFICATE` structure.
  constexpr size_t kSizeofWinCertificateHeader = 8;
  std::vector<uint8_t> win_certificate_header(kSizeofWinCertificateHeader);
  const uint32_t certs_size = kSizeofWinCertificateHeader + ret->size();
  memcpy(&win_certificate_header[0], &certs_size, sizeof(certs_size));
  memcpy(&win_certificate_header[4], &kAttributeCertificateRevision,
         sizeof(kAttributeCertificateRevision));
  memcpy(&win_certificate_header[6], &kAttributeCertificateTypePKCS7SignedData,
         sizeof(kAttributeCertificateTypePKCS7SignedData));

  ret->insert(ret->begin(), win_certificate_header.begin(),
              win_certificate_header.end());
  ret->insert(ret->begin(), binary_.data(), binary_.data() + attr_cert_offset_);

  // Inject the updated length in the `IMAGE_DATA_DIRECTORY` structure that
  // delineates the `WIN_CERTIFICATE` structure.
  memcpy(ret->data() + certs_size_offset_, &certs_size, sizeof(certs_size));

  return ret;
}

PEBinary::PEBinary() = default;

bool PEBinary::ParseTag() {
  const auto [success, tag] = ParseTagImpl(content_info_);
  if (tag) {
    tag_ = std::vector<uint8_t>(tag->begin(), tag->end());
  }
  return success;
}

std::optional<SectorFormat> NewSectorFormat(uint16_t sector_shift) {
  const uint64_t sector_size = 1 << sector_shift;
  if (sector_size != 4096 && sector_size != 512) {
    // Unexpected msi sector shift.
    return {};
  }
  return SectorFormat{sector_size, static_cast<int>(sector_size / 4)};
}

bool IsLastInSector(const SectorFormat& format, int index) {
  return index > kNumDifatHeaderEntries &&
         (index - kNumDifatHeaderEntries + 1) % format.ints == 0;
}

MSIDirEntry::MSIDirEntry(const MSIDirEntry&) = default;
MSIDirEntry::MSIDirEntry() = default;
MSIDirEntry::~MSIDirEntry() = default;

MSIHeader::MSIHeader(const MSIHeader&) = default;
MSIHeader::MSIHeader() = default;
MSIHeader::~MSIHeader() = default;

std::vector<uint8_t> MSIBinary::ReadStream(const std::string& name,
                                           uint32_t start,
                                           uint64_t stream_size,
                                           bool force_fat,
                                           bool free_data) {
  uint64_t sector_size = sector_format_.size;
  std::optional<std::vector<uint32_t>> mini_fat_entries;
  std::optional<std::vector<uint8_t>> mini_contents;

  // Code that manages mini fat will probably not run in prod.
  if (!force_fat && stream_size < kMiniStreamCutoffSize) {
    // Load the mini fat.
    std::vector<uint8_t> stream = ReadStream(
        "mini fat", header_.first_mini_fat_sector,
        header_.num_mini_fat_sectors * sector_format_.size, true, false);
    mini_fat_entries = std::vector<uint32_t>();
    for (size_t offset = 0; offset < stream.size(); offset += 4) {
      mini_fat_entries->push_back(
          *reinterpret_cast<uint32_t*>(&stream[offset]));
    }

    // Load the mini stream, the root directory's stream. root must be dir entry
    // zero.
    MSIDirEntry root;
    const uint64_t offset = header_.first_dir_sector * sector_format_.size;
    std::memcpy(&root, &contents_[offset], sizeof(MSIDirEntry));
    mini_contents = ReadStream("mini stream", root.stream_first_sector,
                               root.stream_size, true, false);
    sector_size = kMiniStreamSectorSize;
  }

  std::vector<uint32_t>* fat_entries =
      mini_fat_entries ? &*mini_fat_entries : &fat_entries_;
  std::vector<uint8_t>* contents = mini_contents ? &*mini_contents : &contents_;

  uint32_t sector = start;
  uint64_t size = stream_size;
  std::vector<uint8_t> stream;
  while (size > 0) {
    if (sector == kFatEndOfChain || sector == kFatFreeSector) {
      // Ran out of sectors in copying stream.
      return {};
    }
    uint64_t n = size;
    if (n > sector_size) {
      n = sector_size;
    }
    const uint64_t offset = sector_size * sector;
    stream.insert(stream.end(), contents->begin() + offset,
                  contents->begin() + offset + n);
    size -= n;

    // Zero out the existing stream bytes, if requested.
    // For example, new signedData will be written at the end of the file, which
    // may be where the existing stream is, but this works regardless. The
    // stream bytes could be left as unused junk, but unused bytes in an MSI
    // file are typically zeroed. Set the data in the sector to zero.
    if (free_data) {
      for (uint64_t i = 0; i < n; ++i) {
        (*contents)[offset + i] = 0;
      }
    }

    // Find the next sector, then free the fat entry of the current sector.
    uint32_t old = sector;
    sector = (*fat_entries)[sector];
    if (free_data) {
      (*fat_entries)[old] = kFatFreeSector;
    }
  }
  return stream;
}

void MSIBinary::PopulateFatEntries() {
  std::vector<uint32_t> fat_entries;
  for (size_t i = 0; i < difat_entries_.size(); ++i) {
    const uint32_t sector = difat_entries_[i];

    // The last entry in a difat sector is a chaining entry.
    if (sector == kFatFreeSector || sector == kFatEndOfChain ||
        IsLastInSector(sector_format_, i)) {
      continue;
    }
    const uint64_t offset = sector * sector_format_.size;
    for (int j = 0; j < sector_format_.ints; ++j) {
      fat_entries.push_back(
          *reinterpret_cast<uint32_t*>(&contents_[offset + j * 4]));
    }
  }
  fat_entries_ = fat_entries;
}

void MSIBinary::PopulateDifatEntries() {
  std::vector<uint32_t> difat_entries(kNumDifatHeaderEntries);
  difat_entries.reserve(kNumDifatHeaderEntries +
                        header_.num_difat_sectors * sector_format_.ints);
  for (int i = 0; i < kNumDifatHeaderEntries; ++i) {
    difat_entries[i] = *reinterpret_cast<uint32_t*>(
        &header_bytes_[kNumHeaderContentBytes + i * 4]);
  }

  // Code here that manages additional difat sectors will probably not run in
  // prod, but is implemented to avoid a scaling limit. (109 difat sector
  // entries) x (1024 fat sector entries/difat sector) x (4096
  // bytes/ fat sector)
  // => files greater than ~457 MB in size require additional difat sectors.
  std::vector<uint32_t> difat_sectors;
  for (uint32_t i = 0; i < header_.num_difat_sectors; ++i) {
    uint32_t sector = 0;
    sector = i == 0 ? header_.first_difat_sector
                    : difat_entries[difat_entries.size() - 1];
    difat_sectors.push_back(sector);
    uint64_t start = sector * sector_format_.size;
    for (int j = 0; j < sector_format_.ints; ++j) {
      difat_entries.push_back(
          *reinterpret_cast<uint32_t*>(&contents_[start + j * 4]));
    }
  }
  difat_entries_ = difat_entries;
  difat_sectors_ = difat_sectors;
}

SignedDataDir MSIBinary::SignedDataDirFromSector(uint64_t dir_sector) {
  MSIDirEntry sig_dir_entry;
  for (uint64_t i = 0; i < sector_format_.size / kNumDirEntryBytes; ++i) {
    const uint64_t offset =
        dir_sector * sector_format_.size + i * kNumDirEntryBytes;
    std::memcpy(&sig_dir_entry, &contents_[offset], sizeof(MSIDirEntry));
    if (std::equal(sig_dir_entry.name,
                   sig_dir_entry.name + sig_dir_entry.num_name_bytes,
                   std::begin(kSignatureName))) {
      return {sig_dir_entry, offset, true};
    }
  }
  return {};
}

bool MSIBinary::PopulateSignatureDirEntry() {
  uint64_t dir_sector = header_.first_dir_sector;
  while (true) {
    const auto [sig_dir_entry, offset, found] =
        SignedDataDirFromSector(dir_sector);
    if (found) {
      sig_dir_entry_ = sig_dir_entry;
      sig_dir_offset_ = offset;
      return true;
    }

    // Did not find the entry, go to the next directory sector.
    dir_sector = fat_entries_[dir_sector];
    if (dir_sector == kFatEndOfChain) {
      // Did not find signature stream in MSI file.
      return false;
    }
  }
}

void MSIBinary::PopulateSignedData() {
  signed_data_bytes_ = ReadStream(
      "signedData", sig_dir_entry_.stream_first_sector,
      header_.dll_version != 3 ? sig_dir_entry_.stream_size
                               : sig_dir_entry_.stream_size & 0x7FFFFFFF,
      false, true);
}

void MSIBinary::AssignDifatEntry(uint64_t fat_sector) {
  EnsureFreeDifatEntry();

  // Find first free entry at end of list.
  int i = difat_entries_.size() - 1;

  // If there are sectors, `i` could be pointing to a fat end-of-chain marker,
  // but in that case it is guaranteed by `EnsureFreeDifatEntry()` that the
  // prior element is a free sector, and the following loop works.

  // As long as the prior element is a free sector, decrement `i`.
  // If the prior element is at the end of a difat sector, skip over it.
  while (difat_entries_[i - 1] == kFatFreeSector ||
         (IsLastInSector(sector_format_, i - 1) &&
          difat_entries_[i - 2] == kFatFreeSector)) {
    --i;
  }
  difat_entries_[i] = fat_sector;
}

void MSIBinary::EnsureFreeDifatEntry() {
  // By construction, `difat_entries_` is at least `kNumDifatHeaderEntries`
  // long.
  int i = difat_entries_.size() - 1;
  if (difat_entries_[i] == kFatEndOfChain) {
    --i;
  }
  if (difat_entries_[i] == kFatFreeSector) {
    return;
  }
  const int old_difat_tail = difat_entries_.size() - 1;

  // Allocate another sector of difat entries.
  for (i = 0; i < sector_format_.ints; ++i) {
    difat_entries_.push_back(kFatFreeSector);
  }
  difat_entries_[difat_entries_.size() - 1] = kFatEndOfChain;

  // Assign the new difat sector in the fat.
  uint64_t sector = EnsureFreeFatEntries(1);
  fat_entries_[sector] = kFatDifSector;

  // Assign the "next sector" pointer in the previous sector or header.
  if (!header_.num_difat_sectors) {
    header_.first_difat_sector = sector;
  } else {
    difat_entries_[old_difat_tail] = sector;
  }
  ++header_.num_difat_sectors;

  difat_sectors_.push_back(sector);
}

// static
uint64_t MSIBinary::FirstFreeFatEntry(const std::vector<uint32_t>& entries) {
  uint64_t first_free_index = entries.size();
  while (entries[first_free_index - 1] == kFatFreeSector) {
    --first_free_index;
  }
  return first_free_index;
}

uint64_t MSIBinary::FirstFreeFatEntry() {
  return FirstFreeFatEntry(fat_entries_);
}

uint64_t MSIBinary::EnsureFreeFatEntries(uint64_t n) {
  uint64_t size_fat = fat_entries_.size();
  uint64_t first_free_index = FirstFreeFatEntry();
  if (size_fat - first_free_index >= n) {
    // Nothing to do, there were already enough free sectors.
    return first_free_index;
  }

  // Append another fat sector.
  for (int i = 0; i < sector_format_.ints; ++i) {
    fat_entries_.push_back(kFatFreeSector);
  }

  // `first_free_index` is free; assign it to the created fat sector.
  // Do not change the order of these calls, since `AssignDifatEntry()` could
  // invalidate `first_free_index`.
  fat_entries_[first_free_index] = kFatFatSector;
  AssignDifatEntry(first_free_index);

  // Update the MSI header.
  ++header_.num_fat_sectors;

  // If n is large enough, it is possible adding an additional sector was
  // insufficient. This will not happen for our use case, but the call to verify
  // or fix it is cheap.
  EnsureFreeFatEntries(n);

  return FirstFreeFatEntry();
}

MSIBinary::MSIBinary(const MSIBinary&) = default;
MSIBinary::MSIBinary() = default;
MSIBinary::~MSIBinary() = default;

// static
std::unique_ptr<MSIBinary> MSIBinary::Parse(
    base::span<const uint8_t> file_contents) {
  if (file_contents.size() < kNumHeaderTotalBytes) {
    // MSI file is too short to contain header.
    return {};
  }
  auto msi_binary = std::make_unique<MSIBinary>();

  // Parse the header.
  msi_binary->header_bytes_ = std::vector<uint8_t>(
      file_contents.begin(), file_contents.begin() + kNumHeaderTotalBytes);
  std::memcpy(&msi_binary->header_, &msi_binary->header_bytes_[0],
              sizeof(MSIHeader));
  if (std::memcmp(msi_binary->header_.magic, kMsiHeaderSignature,
                  sizeof(kMsiHeaderSignature)) != 0 ||
      std::memcmp(msi_binary->header_.clsid, kMsiHeaderClsid,
                  sizeof(kMsiHeaderClsid)) != 0) {
    // Not an msi file.
    return {};
  }
  const auto sector_format = NewSectorFormat(msi_binary->header_.sector_shift);
  if (!sector_format) {
    return {};
  }
  msi_binary->sector_format_ = *sector_format;
  if (file_contents.size() < msi_binary->sector_format_.size) {
    // MSI file is too short to contain a full header sector.
    return {};
  }
  msi_binary->contents_ = std::vector<uint8_t>(
      file_contents.begin() + msi_binary->sector_format_.size,
      file_contents.end());

  // The difat entries must be populated before the fat entries.
  msi_binary->PopulateDifatEntries();
  msi_binary->PopulateFatEntries();

  // The signature dir entry must be populated before the signed data.
  if (!msi_binary->PopulateSignatureDirEntry()) {
    return {};
  }
  msi_binary->PopulateSignedData();

  if (!msi_binary->ParseTag()) {
    return {};
  }

  return msi_binary;
}

std::vector<uint8_t> MSIBinary::BuildBinary(
    const std::vector<uint8_t>& signed_data) {
  if (signed_data.size() < kMiniStreamCutoffSize) {
    // Writing SignedData less than 4096 bytes is not supported.
    return {};
  }

  // Ensure enough free fat entries for the signedData.
  const uint64_t num_signed_data_sectors =
      (signed_data.size() - 1) / sector_format_.size + 1;
  const uint64_t first_signed_data_sector =
      EnsureFreeFatEntries(num_signed_data_sectors);

  // Allocate sectors for the signedData, in a copy of the fat entries.
  std::vector<uint32_t> new_fat_entries = fat_entries_;
  for (uint64_t i = 0; i < num_signed_data_sectors - 1; ++i) {
    new_fat_entries[first_signed_data_sector + i] =
        first_signed_data_sector + i + 1;
  }
  new_fat_entries[first_signed_data_sector + num_signed_data_sectors - 1] =
      kFatEndOfChain;

  // Update the signedData stream's directory entry location and size, in copy
  // of dir entry.
  MSIDirEntry new_sig_dir_entry = sig_dir_entry_;
  new_sig_dir_entry.stream_first_sector = first_signed_data_sector;
  new_sig_dir_entry.stream_size = signed_data.size();
  const size_t signed_data_offset =
      first_signed_data_sector * sector_format_.size;

  // Write out the...
  // ...header,
  std::vector<uint8_t> header_sector_bytes(sector_format_.size);
  std::memcpy(&header_sector_bytes[0], &header_, sizeof(MSIHeader));
  for (int i = 0; i < kNumDifatHeaderEntries; ++i) {
    std::memcpy(&header_sector_bytes[kNumHeaderContentBytes + i * 4],
                &difat_entries_[i], sizeof(uint32_t));
  }

  // ...content,
  // Make a copy of the content bytes, since new data will be overlaid on it.
  const size_t new_contents_size =
      sector_format_.size * FirstFreeFatEntry(new_fat_entries);
  CHECK_GT(new_contents_size, signed_data_offset + signed_data.size());
  std::vector<uint8_t> new_contents(new_contents_size);
  std::memcpy(&new_contents[0], &contents_[0], signed_data_offset);

  // ...signedData directory entry from local modified copy,
  std::memcpy(&new_contents[sig_dir_offset_], &new_sig_dir_entry,
              sizeof(MSIDirEntry));

  // ...difat entries,
  // In case difat sectors were added for huge files.
  for (size_t i = 0; i < difat_sectors_.size(); ++i) {
    const int index = kNumDifatHeaderEntries + i * sector_format_.ints;
    uint64_t offset = difat_sectors_[i] * sector_format_.size;
    for (int j = 0; j < sector_format_.ints; ++j) {
      std::memcpy(&new_contents[offset + j * 4], &difat_entries_[index + j],
                  sizeof(uint32_t));
    }
  }

  // ...fat entries from local modified copy,
  int index = 0;
  for (size_t i = 0; i < difat_entries_.size(); ++i) {
    if (difat_entries_[i] != kFatFreeSector &&
        difat_entries_[i] != kFatEndOfChain &&
        !IsLastInSector(sector_format_, i)) {
      const uint64_t offset = difat_entries_[i] * sector_format_.size;
      for (int j = 0; j < sector_format_.ints; ++j) {
        std::memcpy(&new_contents[offset + j * 4], &new_fat_entries[index + j],
                    sizeof(uint32_t));
      }
      index += sector_format_.ints;
    }
  }

  // ...signedData
  // `new_contents` is zero-initialized, so no need to add padding to end of
  // sector. The sectors allocated for signedData are guaranteed contiguous.
  std::memcpy(&new_contents[signed_data_offset], &signed_data[0],
              signed_data.size());

  // ...finally, build and return the new binary.
  std::vector<uint8_t> binary(header_sector_bytes.size() + new_contents.size());
  std::memcpy(&binary[0], &header_sector_bytes[0], header_sector_bytes.size());
  std::memcpy(&binary[header_sector_bytes.size()], &new_contents[0],
              new_contents.size());
  return binary;
}

std::optional<std::vector<uint8_t>> MSIBinary::SetTag(
    base::span<const uint8_t> tag) {
  std::optional<std::vector<uint8_t>> signed_data =
      SetTagImpl(signed_data_bytes_, tag);
  if (!signed_data) {
    return {};
  }

  return BuildBinary(*signed_data);
}

bool MSIBinary::ParseTag() {
  const auto [success, tag] = ParseTagImpl(signed_data_bytes_);
  if (tag) {
    tag_ = std::vector<uint8_t>(tag->begin(), tag->end());
  }
  return success;
}

std::optional<std::vector<uint8_t>> MSIBinary::tag() const {
  return tag_;
}

}  // namespace internal

std::unique_ptr<BinaryInterface> CreatePEBinary(
    base::span<const uint8_t> contents) {
  return internal::PEBinary::Parse(contents);
}
std::unique_ptr<BinaryInterface> CreateMSIBinary(
    base::span<const uint8_t> contents) {
  return internal::MSIBinary::Parse(contents);
}

}  // namespace updater::tagging
