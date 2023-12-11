// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"

namespace updater::tagging {

namespace {
// PEBinary represents a Windows PE binary and provides functions to extract and
// set data outside of the signed area (called a "tag"). This allows a binary to
// contain arbitrary data without invalidating any Authenticode signature.
class PEBinary : public BinaryInterface {
 public:
  PEBinary(const PEBinary&);
  PEBinary();
  ~PEBinary() override;

  // Parse a signed, Windows PE binary. Note that the returned structure
  // contains pointers into the given data.
  static std::unique_ptr<BinaryInterface> Parse(
      base::span<const uint8_t> binary);

  // Returns the embedded tag, if any.
  std::optional<std::vector<const uint8_t>> tag() const override;

  // SetTag returns an updated version of the PE binary image that contains the
  // given tag, or |nullopt| on error. If the binary already contains a tag then
  // it will be replaced.
  std::optional<std::vector<uint8_t>> SetTag(
      base::span<const uint8_t> tag) override;

 private:
  // ParseTag attempts to parse out the tag. It returns false on parse error or
  // true on success. If successful, it sets |tag_|.
  bool ParseTag();

  // binary_ contains the whole input binary.
  base::span<const uint8_t> binary_;

  // content_info_ contains the |WIN_CERTIFICATE| structure.
  base::span<const uint8_t> content_info_;

  // tag_ contains the embedded tag, or |nullopt| if there isn't one.
  std::optional<std::vector<const uint8_t>> tag_;

  // attr_cert_offset_ is the offset in the file where the |WIN_CERTIFICATE|
  // structure appears. (This is the last structure in the file.)
  size_t attr_cert_offset_ = 0;

  // certs_size_offset_ is the offset in the file where the u32le size of the
  // |WIN_CERTIFICATE| structure is embedded in an |IMAGE_DATA_DIRECTORY|.
  size_t certs_size_offset_ = 0;
};

#pragma pack(push)
#pragma pack(1)

// SectorFormat represents parameters of an MSI file sector.
struct SectorFormat {
  // the size of a sector in bytes; 512 for dll v3 and 4096 for v4.
  uint64_t size = 0;

  // the number of int32s in a sector.
  int ints = 0;
};

// MSIDirEntry represents a parsed MSI directory entry for a stream.
struct MSIDirEntry {
  MSIDirEntry(const MSIDirEntry&);
  MSIDirEntry();
  ~MSIDirEntry();
  char name[64] = {};
  uint16_t num_name_bytes = 0;
  uint8_t object_type = 0;
  uint8_t color_flag = 0;
  uint32_t left = 0;
  uint32_t right = 0;
  uint32_t child = 0;
  uint8_t clsid[16] = {};
  uint32_t state_flags = 0;
  uint64_t create_time = 0;
  uint64_t modify_time = 0;
  uint32_t stream_first_sector = 0;
  uint64_t stream_size = 0;
};

// MSIHeader represents a parsed MSI header.
struct MSIHeader {
  MSIHeader(const MSIHeader&);
  MSIHeader();
  ~MSIHeader();
  uint8_t magic[8] = {};
  uint8_t clsid[16] = {};
  uint16_t minor_version = 0;
  uint16_t dll_version = 0;
  uint16_t byte_order = 0;
  uint16_t sector_shift = 0;
  uint16_t mini_sector_shift = 0;
  uint8_t reserved[6] = {};
  uint32_t num_dir_sectors = 0;
  uint32_t num_fat_sectors = 0;
  uint32_t first_dir_sector = 0;
  uint32_t transaction_signature_number = 0;
  uint32_t mini_stream_cutoff_size = 0;
  uint32_t first_mini_fat_sector = 0;
  uint32_t num_mini_fat_sectors = 0;
  uint32_t first_difat_sector = 0;
  uint32_t num_difat_sectors = 0;
};

struct SignedDataDir {
  MSIDirEntry sig_dir_entry;
  uint64_t offset = 0;
  bool found = false;
};
#pragma pack(pop)

// MSIBinary represents a Windows MSI binary and provides functions to extract
// and set a "tag" outside of the signed area. This allows the MSI to contain
// arbitrary data without invalidating any Authenticode signature.
class MSIBinary : public BinaryInterface {
 public:
  MSIBinary(const MSIBinary&);
  MSIBinary();
  ~MSIBinary() override;

  // Parses the MSI header, the directory entry for the SignedData, and the
  // SignedData itself. If successful, returns a `BinaryInterface` to the
  // `MSIBinary` object.
  static std::unique_ptr<BinaryInterface> Parse(
      base::span<const uint8_t> file_contents);

  // Returns the embedded tag, if any.
  std::optional<std::vector<const uint8_t>> tag() const override;

  // Returns a complete MSI binary image based on bin, but where the superfluous
  // certificate contains the given tag data.
  std::optional<std::vector<uint8_t>> SetTag(
      base::span<const uint8_t> tag) override;

 private:
  // Builds an MSI binary based on the current `MSIBinary`, but with the given
  // SignedData.
  std::vector<uint8_t> BuildBinary(const std::vector<uint8_t>& signed_data);

  // Populates the superfluous-cert `tag_` if found. Returns `true` if the
  // parsing did not produce any errors, even if a tag was not found.
  bool ParseTag();

  // Reads the stream starting at the given start sector.
  std::vector<uint8_t> ReadStream(const std::string& name,
                                  uint32_t start,
                                  uint64_t stream_size,
                                  bool force_fat,
                                  bool free_data);

  // Parse-time functionality is broken out into Populate*() methods for
  // clarity.

  void PopulateFatEntries();

  // Copy the difat entries and make a list of difat sectors, if any.
  // The first 109 difat entries must exist and are read from the MSI header,
  // the rest come from optional additional sectors.
  void PopulateDifatEntries();

  // Returns the directory entry for the signedData stream, if it exists in the
  // given sector.
  SignedDataDir SignedDataDirFromSector(uint64_t dir_sector);

  bool PopulateSignatureDirEntry();
  void PopulateSignedData();

  // Returns the index of the first free entry at the end of a vector of fat
  // entries. It returns one past the end of list if there are no free entries
  // at the end.
  static uint64_t FirstFreeFatEntry(const std::vector<uint32_t>& entries);
  uint64_t FirstFreeFatEntry();

  // Ensures there are at least n free entries at the end of the FAT list,
  // and returns the first free entry.
  uint64_t EnsureFreeFatEntries(uint64_t n);

  // The header (512 bytes).
  std::vector<uint8_t> header_bytes_;

  // The parsed msi header.
  MSIHeader header_;

  // sector parameters.
  SectorFormat sector_format_;

  // The file contents with no header.
  std::vector<uint8_t> contents_;

  // The offset of the signedData stream directory in `contents_`.
  uint64_t sig_dir_offset_ = 0;

  // The parsed contents of the signedData stream directory.
  MSIDirEntry sig_dir_entry_;

  // The PKCS#7, SignedData in asn1 DER form.
  std::vector<uint8_t> signed_data_bytes_;

  // A copy of the FAT entries in one list.
  std::vector<uint32_t> fat_entries_;

  // A copy of the DIFAT entries in one list.
  std::vector<uint32_t> difat_entries_;

  // A list of the dedicated DIFAT sectors, if any, for convenience.
  std::vector<uint32_t> difat_sectors_;

  // The parsed tag, if any.
  std::optional<std::vector<const uint8_t>> tag_;
};

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

PEBinary::PEBinary(const PEBinary&) = default;
PEBinary::~PEBinary() = default;

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
std::unique_ptr<BinaryInterface> PEBinary::Parse(
    base::span<const uint8_t> binary) {
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
    NOTREACHED();
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

std::optional<std::vector<const uint8_t>> PEBinary::tag() const {
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

struct ParseResult {
  bool success = false;
  std::optional<base::span<const uint8_t>> tag;
};

// Parses the `signed_data` PKCS7 object to find the final certificate in the
// list and see whether it has an extension with `kTagOID`, and if so, returns a
// `base::span` of the tag within this `signed_data`. `success` is set to `true`
// if there were no parse errors, even if a tag could not be found.
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

// Returns an updated version of the ContentInfo signedData PKCS7 object with
// the given `new_tag` added, or `nullopt` on error. If the input `signed_data`
// already contains a tag, then it will be replaced with `new_tag`.
std::optional<std::vector<uint8_t>> SetTagImpl(
    base::span<const uint8_t> signed_data,
    base::span<const uint8_t> new_tag) {
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
      !CBB_add_bytes(&tag_cbb, new_tag.data(), new_tag.size()) ||
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
    tag_ = std::vector<const uint8_t>(tag->begin(), tag->end());
  }
  return success;
}

// Compound file binary format constants.
constexpr int kNumHeaderContentBytes = 76;
constexpr int kNumHeaderTotalBytes = 512;
constexpr int kNumDifatHeaderEntries = 109;
constexpr int kNumDirEntryBytes = 128;
constexpr int kMiniStreamCutoffSize = 4096;

// An unallocated sector (used in the FAT or DIFAT).
constexpr uint32_t kFatFreeSector = 0xFFFFFFFF;

// End of a linked chain (in the FAT); or end of DIFAT sector chain.
constexpr uint32_t kFatEndOfChain = 0xFFFFFFFE;

constexpr uint8_t kMsiHeaderSignature[] = {0xd0, 0xcf, 0x11, 0xe0,
                                           0xa1, 0xb1, 0x1a, 0xe1};
constexpr uint8_t kMsiHeaderClsid[16] = {};

// UTF-16 for "\05DigitalSignature".
constexpr uint8_t kSignatureName[] = {
    0x05, 0x00, 0x44, 0x00, 0x69, 0x00, 0x67, 0x00, 0x69, 0x00, 0x74, 0x00,
    0x61, 0x00, 0x6c, 0x00, 0x53, 0x00, 0x69, 0x00, 0x67, 0x00, 0x6e, 0x00,
    0x61, 0x00, 0x74, 0x00, 0x75, 0x00, 0x72, 0x00, 0x65, 0x00, 0x00, 0x00};

std::optional<SectorFormat> NewSectorFormat(uint16_t sector_shift) {
  const uint64_t sector_size = 1 << sector_shift;
  if (sector_size != 4096 && sector_size != 512) {
    // Unexpected msi sector shift.
    return {};
  }
  return SectorFormat{sector_size, static_cast<int>(sector_size / 4)};
}

// Returns whether the index corresponds to the last entry in a sector.
//
// The last entry in each difat sector is a pointer to the next difat sector, or
// is an end-of-chain marker.
// This does not apply to the last entry stored in the MSI header.
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
  // TODO(crbug.com/1422360): handle the mini FAT format.
  CHECK(force_fat || stream_size >= kMiniStreamCutoffSize);

  uint32_t sector = start;
  uint64_t size = stream_size;
  std::vector<uint8_t> stream;
  while (size > 0) {
    if (sector == kFatEndOfChain || sector == kFatFreeSector) {
      // Ran out of sectors in copying stream.
      return {};
    }
    uint64_t n = size;
    if (n > sector_format_.size) {
      n = sector_format_.size;
    }
    const uint64_t offset = sector_format_.size * sector;
    stream.insert(stream.end(), contents_.begin() + offset,
                  contents_.begin() + offset + n);
    size -= n;

    // Zero out the existing stream bytes, if requested.
    // For example, new signedData will be written at the end of the file, which
    // may be where the existing stream is, but this works regardless. The
    // stream bytes could be left as unused junk, but unused bytes in an MSI
    // file are typically zeroed. Set the data in the sector to zero.
    if (free_data) {
      for (uint64_t i = 0; i < n; ++i) {
        contents_[offset + i] = 0;
      }
    }

    // Find the next sector, then free the FAT entry of the current sector.
    uint32_t old = sector;
    sector = fat_entries_[sector];
    if (free_data) {
      fat_entries_[old] = kFatFreeSector;
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

  // TODO(crbug.com/1422360): handle additional difat sectors.
  CHECK(!header_.num_difat_sectors);
  difat_entries_ = difat_entries;
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

// static
uint64_t MSIBinary::FirstFreeFatEntry(const std::vector<uint32_t>& entries) {
  uint64_t first_free_index = entries.size();
  while (entries[first_free_index - 1] == kFatFreeSector) {
    first_free_index--;
  }
  return first_free_index;
}

uint64_t MSIBinary::FirstFreeFatEntry() {
  return FirstFreeFatEntry(fat_entries_);
}

uint64_t MSIBinary::EnsureFreeFatEntries(uint64_t n) {
  uint64_t size_fat = fat_entries_.size();
  uint64_t first_free_index = FirstFreeFatEntry();

  // TODO(crbug.com/1422360): handle adding additional FAT sectors.
  CHECK_GE(size_fat - first_free_index, n);

  return first_free_index;
}

MSIBinary::MSIBinary(const MSIBinary&) = default;
MSIBinary::MSIBinary() = default;
MSIBinary::~MSIBinary() = default;

// static
std::unique_ptr<BinaryInterface> MSIBinary::Parse(
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

  // Ensure enough free FAT entries for the signedData.
  const uint64_t num_signed_data_sectors =
      (signed_data.size() - 1) / sector_format_.size + 1;
  const uint64_t first_signed_data_sector =
      EnsureFreeFatEntries(num_signed_data_sectors);

  // Allocate sectors for the signedData, in a copy of the FAT entries.
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
  std::vector<uint8_t> new_contents(sector_format_.size *
                                    FirstFreeFatEntry(new_fat_entries));
  std::memcpy(&new_contents[0], &contents_[0], contents_.size());

  // ...signedData directory entry from local modified copy,
  std::memcpy(&new_contents[sig_dir_offset_], &new_sig_dir_entry,
              sizeof(MSIDirEntry));

  // TODO(crbug.com/1422360): handle additional difat sectors.
  CHECK(!difat_sectors_.size());

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
  std::memcpy(&new_contents[first_signed_data_sector * sector_format_.size],
              &signed_data[0], signed_data.size());

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
    tag_ = std::vector<const uint8_t>(tag->begin(), tag->end());
  }
  return success;
}

std::optional<std::vector<const uint8_t>> MSIBinary::tag() const {
  return tag_;
}

}  // namespace

std::unique_ptr<BinaryInterface> CreatePEBinary(
    base::span<const uint8_t> contents) {
  return PEBinary::Parse(contents);
}
std::unique_ptr<BinaryInterface> CreateMSIBinary(
    base::span<const uint8_t> contents) {
  return MSIBinary::Parse(contents);
}

}  // namespace updater::tagging
