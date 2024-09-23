// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CERTIFICATE_TAG_INTERNAL_H_
#define CHROME_UPDATER_CERTIFICATE_TAG_INTERNAL_H_

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_span.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"

namespace updater::tagging::internal {

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
  static std::unique_ptr<PEBinary> Parse(base::span<const uint8_t> binary);

  // Returns the embedded tag, if any.
  std::optional<std::vector<uint8_t>> tag() const override;

  // SetTag returns an updated version of the PE binary image that contains the
  // given tag, or `nullopt` on error. If the binary already contains a tag then
  // it will be replaced.
  std::optional<std::vector<uint8_t>> SetTag(
      base::span<const uint8_t> tag) override;

 private:
  // ParseTag attempts to parse out the tag. It returns false on parse error or
  // true on success. If successful, it sets `tag_`.
  bool ParseTag();

  // binary_ contains the whole input binary.
  base::raw_span<const uint8_t> binary_;

  // content_info_ contains the `WIN_CERTIFICATE` structure.
  base::raw_span<const uint8_t> content_info_;

  // tag_ contains the embedded tag, or `nullopt` if there isn't one.
  std::optional<std::vector<uint8_t>> tag_;

  // attr_cert_offset_ is the offset in the file where the `WIN_CERTIFICATE`
  // structure appears. (This is the last structure in the file.)
  size_t attr_cert_offset_ = 0;

  // certs_size_offset_ is the offset in the file where the u32le size of the
  // `WIN_CERTIFICATE` structure is embedded in an `IMAGE_DATA_DIRECTORY`.
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
  static std::unique_ptr<MSIBinary> Parse(
      base::span<const uint8_t> file_contents);

  // Returns the embedded tag, if any.
  std::optional<std::vector<uint8_t>> tag() const override;

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

  // Assigns an entry, the sector# of a fat sector, to the end of the difat
  // list.
  void AssignDifatEntry(uint64_t fat_sector);

  // Ensures there is at least one free entry at the end of the difat list.
  void EnsureFreeDifatEntry();

  // Returns the index of the first free entry at the end of a vector of fat
  // entries. It returns one past the end of list if there are no free entries
  // at the end.
  static uint64_t FirstFreeFatEntry(const std::vector<uint32_t>& entries);
  uint64_t FirstFreeFatEntry();

  // Ensures there are at least n free entries at the end of the fat list,
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

  // A copy of the fat entries in one list.
  std::vector<uint32_t> fat_entries_;

  // A copy of the difat entries in one list.
  std::vector<uint32_t> difat_entries_;

  // A list of the dedicated difat sectors, if any, for convenience.
  std::vector<uint32_t> difat_sectors_;

  // The parsed tag, if any.
  std::optional<std::vector<uint8_t>> tag_;

  FRIEND_TEST_ALL_PREFIXES(CertificateTagMsiFirstFreeFatEntryTest, TestCases);
  FRIEND_TEST_ALL_PREFIXES(CertificateTagMsiEnsureFreeDifatEntryTest,
                           TestCases);
  FRIEND_TEST_ALL_PREFIXES(CertificateTagMsiEnsureFreeFatEntriesTest,
                           TestCases);
  FRIEND_TEST_ALL_PREFIXES(CertificateTagMsiAssignDifatEntryTest, TestCases);
  friend MSIBinary GetMsiBinary(const std::vector<uint32_t>& fat_entries,
                                const std::vector<uint32_t>& difat_entries);
  friend void Validate(
      const MSIBinary& bin,
      std::optional<std::reference_wrapper<const MSIBinary>> other);
};

// CBS is a structure from BoringSSL used for parsing binary and ASN.1-based
// formats. This implementation detail is not exposed in the interface of this
// code so these utility functions convert to/from base::span.
CBS CBSFromSpan(base::span<const uint8_t> span);
base::span<const uint8_t> SpanFromCBS(const CBS* cbs);

// kTagOID contains the DER-serialised form of the extension OID that we stuff
// the tag into: 1.3.6.1.4.1.11129.2.1.9999.
inline constexpr uint8_t kTagOID[] = {0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6,
                                      0x79, 0x02, 0x01, 0xce, 0x0f};

// Certificate constants. See
// http://msdn.microsoft.com/en-us/library/ms920091.aspx.
//
// Despite MSDN claiming that 0x100 is the only, current revision - in
// practice it's 0x200.
inline constexpr uint16_t kAttributeCertificateRevision = 0x200;
inline constexpr uint16_t kAttributeCertificateTypePKCS7SignedData = 2;

// AddName appends an X.500 Name structure to |cbb| containing a single
// commonName with the given value.
bool AddName(CBB* cbb, const char* common_name);

// CopyASN1 copies a single ASN.1 element from |in| to |out|.
bool CopyASN1(CBB* out, CBS* in);

struct ParseResult {
  bool success = false;
  std::optional<base::span<const uint8_t>> tag;
};

// Parses the `signed_data` PKCS7 object to find the final certificate in the
// list and see whether it has an extension with `kTagOID`, and if so, returns a
// `base::span` of the tag within this `signed_data`. `success` is set to `true`
// if there were no parse errors, even if a tag could not be found.
ParseResult ParseTagImpl(base::span<const uint8_t> signed_data);

// Returns an updated version of the ContentInfo signedData PKCS7 object with
// the given `tag` added, or `nullopt` on error. If the input `signed_data`
// already contains a tag, then it will be replaced with `tag`.
std::optional<std::vector<uint8_t>> SetTagImpl(
    base::span<const uint8_t> signed_data,
    base::span<const uint8_t> tag);

// Compound file binary format constants.
inline constexpr int kNumHeaderContentBytes = 76;
inline constexpr int kNumHeaderTotalBytes = 512;
inline constexpr int kNumDifatHeaderEntries = 109;
inline constexpr int kNumDirEntryBytes = 128;
inline constexpr int kMiniStreamSectorSize = 64;
inline constexpr int kMiniStreamCutoffSize = 4096;

// An unallocated sector (used in the fat or difat).
inline constexpr uint32_t kFatFreeSector = 0xFFFFFFFF;

// End of a linked chain (in the fat); or end of difat sector chain.
inline constexpr uint32_t kFatEndOfChain = 0xFFFFFFFE;

// Used in the fat table to indicate a fat sector entry.
inline constexpr uint32_t kFatFatSector = 0xFFFFFFFD;

// Used in the fat table to indicate a difat sector entry.
inline constexpr uint32_t kFatDifSector = 0xFFFFFFFC;

// Reserved value.
inline constexpr uint32_t kFatReserved = 0xFFFFFFFB;

inline constexpr uint8_t kMsiHeaderSignature[] = {0xd0, 0xcf, 0x11, 0xe0,
                                                  0xa1, 0xb1, 0x1a, 0xe1};
inline constexpr uint8_t kMsiHeaderClsid[16] = {};

// UTF-16 for "\05DigitalSignature".
inline constexpr uint8_t kSignatureName[] = {
    0x05, 0x00, 0x44, 0x00, 0x69, 0x00, 0x67, 0x00, 0x69, 0x00, 0x74, 0x00,
    0x61, 0x00, 0x6c, 0x00, 0x53, 0x00, 0x69, 0x00, 0x67, 0x00, 0x6e, 0x00,
    0x61, 0x00, 0x74, 0x00, 0x75, 0x00, 0x72, 0x00, 0x65, 0x00, 0x00, 0x00};

std::optional<SectorFormat> NewSectorFormat(uint16_t sector_shift);

// Returns whether the index corresponds to the last entry in a sector.
//
// The last entry in each difat sector is a pointer to the next difat sector, or
// is an end-of-chain marker.
// This does not apply to the last entry stored in the MSI header.
bool IsLastInSector(const SectorFormat& format, int index);

}  // namespace updater::tagging::internal

#endif  // CHROME_UPDATER_CERTIFICATE_TAG_INTERNAL_H_
