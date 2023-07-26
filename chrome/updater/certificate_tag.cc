// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/certificate_tag.h"

#include "base/notreached.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"

namespace updater::tagging {

// CBS is a structure from BoringSSL used for parsing binary and ASN.1-based
// formats. This implementation detail is not exposed in the interface of this
// code so these utility functions convert to/from base::span.

static CBS CBSFromSpan(const base::span<const uint8_t>& span) {
  CBS cbs;
  CBS_init(&cbs, span.data(), span.size());
  return cbs;
}

static base::span<const uint8_t> SpanFromCBS(const CBS* cbs) {
  return base::span<const uint8_t>(CBS_data(cbs), CBS_len(cbs));
}

Binary::Binary(const Binary&) = default;
Binary::~Binary() = default;

// kTagOID contains the DER-serialised form of the extension OID that we stuff
// the tag into: 1.3.6.1.4.1.11129.2.1.9999.
static constexpr uint8_t kTagOID[] = {0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6,
                                      0x79, 0x02, 0x01, 0xce, 0x0f};

// Certificate constants. See
// http://msdn.microsoft.com/en-us/library/ms920091.aspx.
//
// Despite MSDN claiming that 0x100 is the only, current revision - in
// practice it's 0x200.
static constexpr uint16_t kAttributeCertificateRevision = 0x200;
static constexpr uint16_t kAttributeCertificateTypePKCS7SignedData = 2;

// static
absl::optional<Binary> Binary::Parse(base::span<const uint8_t> binary) {
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
    return absl::nullopt;
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
      return absl::nullopt;
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
    return absl::nullopt;
  }

  CBS bin_for_certs = bin;
  CBS certs;
  if (!CBS_skip(&bin_for_certs, cert_entry_virtual_addr) ||
      !CBS_get_bytes(&bin_for_certs, &certs, cert_entry_size)) {
    return absl::nullopt;
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
    return absl::nullopt;
  }

  Binary ret;
  ret.certs_size_offset_ =
      pe_offset + 4 + kFileHeaderSize + size_of_optional_header -
      8 * (num_directory_entries - kCertificateTableIndex) + 4;

  // Double-check that the calculated |certs_size_offset_| is correct by reading
  // from that location and checking that the value is as expected.
  uint32_t cert_entry_size_duplicate = 0;
  CBS bin_for_check = bin;
  if (!CBS_skip(&bin_for_check, ret.certs_size_offset_) ||
      !CBS_get_u32le(&bin_for_check, &cert_entry_size_duplicate) ||
      cert_entry_size_duplicate != cert_entry_size) {
    NOTREACHED();
    return absl::nullopt;
  }

  ret.binary_ = binary;
  ret.content_info_ = SpanFromCBS(&signed_data);
  ret.attr_cert_offset_ = cert_entry_virtual_addr;

  if (!ret.ParseTag()) {
    return absl::nullopt;
  }

  return ret;
}

const absl::optional<base::span<const uint8_t>>& Binary::tag() const {
  return tag_;
}

// AddName appends an X.500 Name structure to |cbb| containing a single
// commonName with the given value.
static bool AddName(CBB* cbb, const char* common_name) {
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

// CopyASN1 copies a single ASN.1 element from |in| to |out|.
static bool CopyASN1(CBB* out, CBS* in) {
  CBS element;
  return CBS_get_any_asn1_element(in, &element, nullptr, nullptr) == 1 &&
         CBB_add_bytes(out, CBS_data(&element), CBS_len(&element)) == 1;
}

absl::optional<std::vector<uint8_t>> Binary::SetTag(
    base::span<const uint8_t> tag) const {
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), binary_.size() + 1024) ||
      // Copy the binary up to the |WIN_CERTIFICATE| structure directly to the
      // output.
      !CBB_add_bytes(cbb.get(), binary_.data(), attr_cert_offset_) ||
      // See the WIN_CERTIFICATE structure from
      // http://msdn.microsoft.com/en-us/library/ms920091.aspx.
      !CBB_add_u32(cbb.get(), 0 /* Length. Filled in later. */) ||
      !CBB_add_u16le(cbb.get(), kAttributeCertificateRevision) ||
      !CBB_add_u16le(cbb.get(), kAttributeCertificateTypePKCS7SignedData)) {
    return absl::nullopt;
  }

  // Walk the PKCS SignedData structure from the input and copy elements to the
  // output until the list of certificates is reached.
  CBS content_info = CBSFromSpan(content_info_);
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
    return absl::nullopt;
  }

  // Copy the certificates from the input to the output, potentially omitting
  // the last one if it's a superfluous cert.
  bool have_last_cert = false;
  CBS last_cert;

  while (CBS_len(&certs) > 0) {
    if ((have_last_cert && !CBB_add_bytes(&certs_cbb, CBS_data(&last_cert),
                                          CBS_len(&last_cert))) ||
        !CBS_get_asn1_element(&certs, &last_cert, CBS_ASN1_SEQUENCE)) {
      return absl::nullopt;
    }
    have_last_cert = true;
  }

  // If there's not already a tag then we need to keep the last certificate.
  // Otherwise it's the certificate with the tag in and we're going to replace
  // it.
  if (!tag_.has_value() &&
      !CBB_add_bytes(&certs_cbb, CBS_data(&last_cert), CBS_len(&last_cert))) {
    return absl::nullopt;
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

  // Create a dummy certificate with an extension containing the tag. See
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
    return absl::nullopt;
  }

  // Copy the CBB result into a std::vector, padding to 8-byte alignment.
  std::vector<uint8_t> ret;
  const size_t padding = (8 - cbb_len % 8) % 8;
  ret.reserve(cbb_len + padding);
  ret.insert(ret.begin(), cbb_data, cbb_data + cbb_len);
  ret.insert(ret.end(), padding, 0);
  OPENSSL_free(cbb_data);

  // Inject the updated length in a couple of places:
  const uint32_t certs_size = cbb_len + padding - attr_cert_offset_;
  //   1) The |IMAGE_DATA_DIRECTORY| structure that delineates the
  //      |WIN_CERTIFICATE| structure.
  memcpy(&ret[certs_size_offset_], &certs_size, sizeof(certs_size));
  //   2) The first word of the |WIN_CERTIFICATE| structure itself.
  memcpy(&ret[attr_cert_offset_], &certs_size, sizeof(certs_size));

  return ret;
}

Binary::Binary() = default;

bool Binary::ParseTag() {
  // Parse the |WIN_CERTIFICATE| structure to find the final certificate in the
  // list and see whether it has an extension with |kTagOID|. If so, that's the
  // tag for this binary.

  CBS content_info = CBSFromSpan(content_info_);
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
    return false;
  }

  bool have_last_cast = false;
  CBS last_cert;

  while (CBS_len(&certs) > 0) {
    if (!CBS_get_asn1(&certs, &last_cert, CBS_ASN1_SEQUENCE)) {
      return false;
    }
    have_last_cast = true;
  }

  if (!have_last_cast) {
    return false;
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
    return false;
  }

  if (!has_extensions) {
    return true;
  }

  CBS extensions;
  if (!CBS_get_asn1(&outer_extensions, &extensions, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  while (CBS_len(&extensions) > 0) {
    CBS extension, oid, contents;
    if (!CBS_get_asn1(&extensions, &extension, CBS_ASN1_SEQUENCE) ||
        !CBS_get_asn1(&extension, &oid, CBS_ASN1_OBJECT) ||
        (CBS_peek_asn1_tag(&extension, CBS_ASN1_BOOLEAN) &&
         !CBS_get_asn1(&extension, nullptr, CBS_ASN1_BOOLEAN)) ||
        !CBS_get_asn1(&extension, &contents, CBS_ASN1_OCTETSTRING) ||
        CBS_len(&extension) != 0) {
      return false;
    }

    if (CBS_len(&oid) == sizeof(kTagOID) &&
        memcmp(CBS_data(&oid), kTagOID, sizeof(kTagOID)) == 0) {
      tag_ = SpanFromCBS(&contents);
      break;
    }
  }

  return true;
}

}  // namespace updater::tagging
