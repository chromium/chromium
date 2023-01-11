// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_parser.h"

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/cbor/reader.h"
#include "components/web_package/input_reader.h"
#include "components/web_package/signed_web_bundles/integrity_block_parser.h"
#include "components/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_util.h"
#include "url/url_constants.h"

namespace web_package {

namespace {

// The number of bytes used to specify the length of the web bundle.
// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-trailing-length
constexpr uint64_t kTrailingLengthNumBytes = 8;

// The maximum size of the section-lengths CBOR item.
constexpr uint64_t kMaxSectionLengthsCBORSize = 8192;

// The maximum size of a metadata section allowed in this implementation.
constexpr uint64_t kMaxMetadataSectionSize = 1 * 1024 * 1024;

// The maximum size of the response header CBOR.
constexpr uint64_t kMaxResponseHeaderLength = 512 * 1024;

// The initial buffer size for reading an item from the response section.
constexpr uint64_t kInitialBufferSizeForResponse = 4096;

// The first byte of WebBundle format >=b2 (Array of length 5).
constexpr uint8_t kBundleHeadByte = 0x85;
// The first byte of WebBundle format b1 (Array of length 6).
constexpr uint8_t kBundleB1HeadByte = 0x86;

// CBOR of the magic string "🌐📦".
// MetadataParser::ParseMagicBytes() checks the first byte (0x85) and this.
//
// The first 10 bytes of the web bundle format are:
//   85                             -- Array of length 5
//      48                          -- Byte string of length 8
//         F0 9F 8C 90 F0 9F 93 A6  -- "🌐📦" in UTF-8
const uint8_t kBundleMagicBytes[] = {
    0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6,
};

// CBOR of the version string "b2\0\0".
//   44               -- Byte string of length 4
//       62 32 00 00  -- "b2\0\0"
const uint8_t kVersionB2MagicBytes[] = {
    0x44, 0x62, 0x32, 0x00, 0x00,
};
// CBOR of the version string "b1\0\0".
//   44               -- Byte string of length 4
//       62 31 00 00  -- "b1\0\0"
const uint8_t kVersionB1MagicBytes[] = {
    0x44, 0x62, 0x31, 0x00, 0x00,
};

// Section names.
constexpr char kCriticalSection[] = "critical";
constexpr char kIndexSection[] = "index";
constexpr char kPrimarySection[] = "primary";
constexpr char kResponsesSection[] = "responses";
constexpr char kSignaturesSection[] = "signatures";

// A list of (section-name, length) pairs.
using SectionLengths = std::vector<std::pair<std::string, uint64_t>>;

// A map from section name to (offset, length) pair.
using SectionOffsets = std::map<std::string, std::pair<uint64_t, uint64_t>>;

bool IsMetadataSection(const std::string& name) {
  return (name == kCriticalSection || name == kIndexSection ||
          name == kPrimarySection || name == kSignaturesSection);
}

// Parses a `section-lengths` CBOR item.
// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-bundle-sections
//   section-lengths = [* (section-name: tstr, length: uint) ]
absl::optional<SectionLengths> ParseSectionLengths(
    base::span<const uint8_t> data) {
  cbor::Reader::DecoderError error;
  absl::optional<cbor::Value> value = cbor::Reader::Read(data, &error);
  if (!value.has_value() || !value->is_array())
    return absl::nullopt;

  const cbor::Value::ArrayValue& array = value->GetArray();
  if (array.size() % 2 != 0)
    return absl::nullopt;

  SectionLengths result;
  for (size_t i = 0; i < array.size(); i += 2) {
    if (!array[i].is_string() || !array[i + 1].is_unsigned())
      return absl::nullopt;
    result.emplace_back(array[i].GetString(), array[i + 1].GetUnsigned());
  }
  return result;
}

struct ParsedHeaders {
  base::flat_map<std::string, std::string> headers;
  base::flat_map<std::string, std::string> pseudos;
};

// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-responses
//   headers = {* bstr => bstr}
absl::optional<ParsedHeaders> ConvertCBORValueToHeaders(
    const cbor::Value& headers_value) {
  // |headers_value| of headers must be a map.
  if (!headers_value.is_map())
    return absl::nullopt;

  ParsedHeaders result;

  for (const auto& item : headers_value.GetMap()) {
    if (!item.first.is_bytestring() || !item.second.is_bytestring())
      return absl::nullopt;
    base::StringPiece name = item.first.GetBytestringAsString();
    base::StringPiece value = item.second.GetBytestringAsString();

    // If name contains any upper-case or non-ASCII characters, return an error.
    // This matches the requirement in Section 8.1.2 of [RFC7540].
    if (!base::IsStringASCII(name) ||
        base::ranges::any_of(name, base::IsAsciiUpper<char>))
      return absl::nullopt;

    if (!name.empty() && name[0] == ':') {
      // pseudos[name] must not exist, because CBOR maps cannot contain
      // duplicate keys. This is ensured by cbor::Reader.
      DCHECK(!result.pseudos.contains(name));
      result.pseudos.insert(
          std::make_pair(std::string(name), std::string(value)));
      continue;
    }

    // Both name and value must be valid.
    if (!net::HttpUtil::IsValidHeaderName(name) ||
        !net::HttpUtil::IsValidHeaderValue(value))
      return absl::nullopt;

    // headers[name] must not exist, because CBOR maps cannot contain duplicate
    // keys. This is ensured by cbor::Reader.
    DCHECK(!result.headers.contains(name));

    result.headers.insert(
        std::make_pair(std::string(name), std::string(value)));
  }

  return result;
}

GURL ParseExchangeURL(base::StringPiece str, const GURL& base_url) {
  DCHECK(base_url.is_empty() || base_url.is_valid());

  if (!base::IsStringUTF8(str))
    return GURL();

  GURL url = base_url.is_valid() ? base_url.Resolve(str) : GURL(str);
  if (!url.is_valid())
    return GURL();

  // Exchange URL must not have a fragment or credentials.
  if (url.has_ref() || url.has_username() || url.has_password())
    return GURL();

  return url;
}

}  // namespace

// A parser for bundle's metadata. This class owns itself and will self destruct
// after calling the ParseMetadataCallback.
class WebBundleParser::MetadataParser
    : WebBundleParser::SharedBundleDataSource::Observer {
 public:
  MetadataParser(scoped_refptr<SharedBundleDataSource> data_source,
                 const GURL& base_url,
                 ParseMetadataCallback callback)
      : data_source_(data_source),
        base_url_(base_url),
        callback_(std::move(callback)) {
    DCHECK(base_url_.is_empty() || base_url_.is_valid());
    data_source_->AddObserver(this);
  }

  MetadataParser(const MetadataParser&) = delete;
  MetadataParser& operator=(const MetadataParser&) = delete;

  ~MetadataParser() override { data_source_->RemoveObserver(this); }

  // Starts parsing of the web bundle. If the data source is backed by a
  // random-access, read the trailing `length` field at the end of the web
  // bundle file and start from that offset.
  // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-trailing-length
  void Start() {
    data_source_->IsRandomAccessContext(base::BindOnce(
        &MetadataParser::OnIsRandomAccessContext, weak_factory_.GetWeakPtr()));
  }

  // Starts parsing of the web bundle at the specified offset, ignoring the
  // `length` field of the web bundle.
  void StartAtOffset(const uint64_t offset) { ReadMagicBytes(offset); }

 private:
  void OnIsRandomAccessContext(const bool is_random_access_context) {
    if (!is_random_access_context) {
      // If the data source is not backed by a random-access context, assume
      // that the web bundle starts at the very first byte of the file and
      // ignore the trailing length field of the bundle.
      ReadMagicBytes(0);
    } else {
      // Otherwise read the length of the file (not the web bundle).
      data_source_->Length(base::BindOnce(&MetadataParser::OnFileLengthRead,
                                          weak_factory_.GetWeakPtr()));
    }
  }
  void OnFileLengthRead(const int64_t file_length) {
    if (file_length < 0) {
      RunErrorCallbackAndDestroy("Error reading bundle length.");
      return;
    }
    if (static_cast<uint64_t>(file_length) < kTrailingLengthNumBytes) {
      RunErrorCallbackAndDestroy("Error reading bundle length.");
      return;
    }

    // Read the last 8 bytes of the file that correspond to the trailing length
    // field of the web bundle.
    data_source_->Read(file_length - kTrailingLengthNumBytes,
                       kTrailingLengthNumBytes,
                       base::BindOnce(&MetadataParser::ParseWebBundleLength,
                                      weak_factory_.GetWeakPtr(), file_length));
  }

  void ParseWebBundleLength(const uint64_t file_length,
                            const absl::optional<std::vector<uint8_t>>& data) {
    if (!data.has_value()) {
      RunErrorCallbackAndDestroy("Error reading bundle length.");
      return;
    }

    // "Recipients loading the bundle in a random-access context SHOULD start by
    // reading the last 8 bytes and seeking backwards by that many bytes to find
    // the start of the bundle, instead of assuming that the start of the file
    // is also the start of the bundle. This allows the bundle to be appended to
    // another format such as a generic self-extracting executable."
    // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#section-4.1.1-3
    InputReader input(*data);
    uint64_t web_bundle_length;
    if (!input.ReadBigEndian(&web_bundle_length)) {
      RunErrorCallbackAndDestroy("Error reading bundle length.");
      return;
    }

    if (web_bundle_length > file_length) {
      RunErrorCallbackAndDestroy("Invalid bundle length.");
      return;
    }
    const uint64_t web_bundle_offset = file_length - web_bundle_length;
    ReadMagicBytes(web_bundle_offset);
  }

  void ReadMagicBytes(const uint64_t offset_in_stream) {
    // First, we will parse the CBOR header of the top level array (1-byte),
    // `magic`, `version`, and the CBOR header of `section-lengths`.
    const uint64_t length = 1 + sizeof(kBundleMagicBytes) +
                            sizeof(kVersionB2MagicBytes) +
                            kMaxCBORItemHeaderSize;
    data_source_->Read(
        offset_in_stream, length,
        base::BindOnce(&MetadataParser::ParseMagicBytes,
                       weak_factory_.GetWeakPtr(), offset_in_stream));
  }

  // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-top-level-structure
  void ParseMagicBytes(uint64_t offset_in_stream,
                       const absl::optional<std::vector<uint8_t>>& data) {
    if (!data) {
      RunErrorCallbackAndDestroy("Error reading bundle magic bytes.");
      return;
    }

    InputReader input(*data);

    // Read the first byte denoting a CBOR array size. It must be equal to 0x85
    // (5).
    const auto array_size = input.ReadByte();
    if (!array_size) {
      RunErrorCallbackAndDestroy("Missing CBOR array size byte.");
      return;
    }

    // Let kBundleB1HeadByte pass this check, to report custom error message for
    // b1 bundles.
    if (*array_size != kBundleHeadByte && *array_size != kBundleB1HeadByte) {
      RunErrorCallbackAndDestroy("Wrong magic bytes.");
      return;
    }

    // Check the magic bytes "48 F0 9F 8C 90 F0 9F 93 A6".
    const auto magic = input.ReadBytes(sizeof(kBundleMagicBytes));
    if (!magic || !base::ranges::equal(*magic, kBundleMagicBytes)) {
      RunErrorCallbackAndDestroy("Wrong magic bytes.");
      return;
    }

    // Let version be the result of reading 5 bytes from stream.
    const auto version = input.ReadBytes(sizeof(kVersionB2MagicBytes));
    if (!version) {
      RunErrorCallbackAndDestroy("Cannot read version bytes.");
      return;
    }
    if (!base::ranges::equal(*version, kVersionB2MagicBytes)) {
      const char* message;
      if (base::ranges::equal(*version, kVersionB1MagicBytes)) {
        message =
            "Bundle format version is 'b1' which is no longer supported."
            " Currently supported version is: 'b2'";
      } else {
        message =
            "Version error: bundle format does not correspond to the specifed "
            "version. Currently supported version is: 'b2'";
      }
      RunErrorCallbackAndDestroy(message,
                                 mojom::BundleParseErrorType::kVersionError);
      return;
    }
    if (*array_size != kBundleHeadByte) {
      RunErrorCallbackAndDestroy(
          "Wrong CBOR array size of the top-level structure");
      return;
    }

    const auto section_lengths_length =
        input.ReadCBORHeader(CBORType::kByteString);
    if (!section_lengths_length) {
      RunErrorCallbackAndDestroy("Cannot parse the size of section-lengths.");
      return;
    }

    // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-top-level-structure
    // "The section-lengths array is embedded in a byte string to facilitate
    // reading it from a network. This byte string MUST be less than 8192
    // (8*1024) bytes long, and parsers MUST NOT load any data from a
    // section-lengths item longer than this."
    if (*section_lengths_length >= kMaxSectionLengthsCBORSize) {
      RunErrorCallbackAndDestroy(
          "The section-lengths CBOR must be smaller than 8192 bytes.");
      return;
    }

    // In the next step, we will parse the content of `section-lengths`,
    // and the CBOR header of `sections`.
    const uint64_t length = *section_lengths_length + kMaxCBORItemHeaderSize;

    offset_in_stream += input.CurrentOffset();
    data_source_->Read(
        offset_in_stream, length,
        base::BindOnce(&MetadataParser::ParseBundleHeader,
                       weak_factory_.GetWeakPtr(), offset_in_stream,
                       *section_lengths_length));
  }

  void ParseBundleHeader(uint64_t offset_in_stream,
                         uint64_t section_lengths_length,
                         const absl::optional<std::vector<uint8_t>>& data) {
    if (!data) {
      RunErrorCallbackAndDestroy("Error reading bundle header.");
      return;
    }
    InputReader input(*data);

    // webbundle = [
    //    magic: h'F0 9F 8C 90 F0 9F 93 A6',
    //    version: bytes .size 4,
    //    section-lengths: bytes .cbor section-lengths,  <==== here
    //    sections: [* any ],
    //    length: bytes .size 8,  ; Big-endian number of bytes in the bundle.
    // ]
    const auto section_lengths_bytes = input.ReadBytes(section_lengths_length);
    if (!section_lengths_bytes) {
      RunErrorCallbackAndDestroy("Cannot read section-lengths.");
      return;
    }
    // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-bundle-sections
    //   section-lengths = [* (section-name: tstr, length: uint) ]
    const auto section_lengths = ParseSectionLengths(*section_lengths_bytes);
    if (!section_lengths) {
      RunErrorCallbackAndDestroy("Cannot parse section-lengths.");
      return;
    }

    // webbundle = [
    //    magic: h'F0 9F 8C 90 F0 9F 93 A6',
    //    version: bytes .size 4,
    //    section-lengths: bytes .cbor section-lengths,
    //    sections: [* any ],  <==== here
    //    length: bytes .size 8,  ; Big-endian number of bytes in the bundle.
    // ]
    const auto num_sections = input.ReadCBORHeader(CBORType::kArray);
    if (!num_sections) {
      RunErrorCallbackAndDestroy("Cannot parse the number of sections.");
      return;
    }

    // "The sections array contains the sections' content. The length of this
    // array MUST be exactly half the length of the section-lengths array, and
    // parsers MUST NOT load any data if that is not the case."
    if (*num_sections != section_lengths->size()) {
      RunErrorCallbackAndDestroy("Unexpected number of sections.");
      return;
    }

    const uint64_t sections_start = offset_in_stream + input.CurrentOffset();
    uint64_t current_offset = sections_start;

    // Convert |section_lengths| to |section_offsets_|.
    for (const auto& pair : *section_lengths) {
      const std::string& name = pair.first;
      const uint64_t length = pair.second;
      bool added = section_offsets_
                       .insert(std::make_pair(
                           name, std::make_pair(current_offset, length)))
                       .second;
      if (!added) {
        RunErrorCallbackAndDestroy("Duplicated section.");
        return;
      }

      if (!base::CheckAdd(current_offset, length)
               .AssignIfValid(&current_offset)) {
        RunErrorCallbackAndDestroy(
            "Integer overflow calculating section offsets.");
        return;
      }
    }

    // "The "responses" section MUST appear after the other three sections
    // defined here, and parsers MUST NOT load any data if that is not the
    // case."
    if (section_lengths->empty() ||
        section_lengths->back().first != kResponsesSection) {
      RunErrorCallbackAndDestroy(
          "Responses section is not the last in section-lengths.");
      return;
    }

    // Initialize |metadata_|.
    metadata_ = mojom::BundleMetadata::New();
    metadata_->version = mojom::BundleFormatVersion::kB2;

    ReadMetadataSections(section_offsets_.begin());
  }

  // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-bundle-sections
  void ReadMetadataSections(SectionOffsets::const_iterator section_iter) {
    for (; section_iter != section_offsets_.end(); ++section_iter) {
      const auto& name = section_iter->first;
      if (!IsMetadataSection(name))
        continue;
      const uint64_t section_offset = section_iter->second.first;
      const uint64_t section_length = section_iter->second.second;
      if (section_length > kMaxMetadataSectionSize) {
        RunErrorCallbackAndDestroy(
            "Metadata sections larger than 1MB are not supported.");
        return;
      }

      data_source_->Read(section_offset, section_length,
                         base::BindOnce(&MetadataParser::ParseMetadataSection,
                                        weak_factory_.GetWeakPtr(),
                                        section_iter, section_length));
      // This loop will be resumed by ParseMetadataSection().
      return;
    }

    // The bundle MUST contain the "index" and "responses" sections.
    if (metadata_->requests.empty()) {
      RunErrorCallbackAndDestroy("Bundle must have an index section.");
      return;
    }

    RunSuccessCallbackAndDestroy();
  }

  void ParseMetadataSection(SectionOffsets::const_iterator section_iter,
                            uint64_t expected_data_length,
                            const absl::optional<std::vector<uint8_t>>& data) {
    if (!data || data->size() != expected_data_length) {
      RunErrorCallbackAndDestroy("Error reading section content.");
      return;
    }

    // Parse the section contents as a CBOR item.
    cbor::Reader::DecoderError error;
    absl::optional<cbor::Value> section_value =
        cbor::Reader::Read(*data, &error);
    if (!section_value) {
      RunErrorCallbackAndDestroy(
          std::string("Error parsing section contents as CBOR: ") +
          cbor::Reader::ErrorCodeToString(error));
      return;
    }

    const auto& name = section_iter->first;
    // Note: Parse*Section() delete |this| on failure.
    if (name == kIndexSection) {
      if (!ParseIndexSection(*section_value))
        return;
    } else if (name == kSignaturesSection) {
      if (!ParseSignaturesSection(*section_value))
        return;
    } else if (name == kCriticalSection) {
      if (!ParseCriticalSection(*section_value))
        return;
    } else if (name == kPrimarySection) {
      if (!ParsePrimarySection(*section_value))
        return;
    } else {
      NOTREACHED();
    }
    // Read the next metadata section.
    ReadMetadataSections(++section_iter);
  }

  // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-the-index-section
  // The index section has the following structure:
  //   index = {* whatwg-url => [ location-in-responses ] }
  //   location-in-responses = (offset: uint, length: uint)
  bool ParseIndexSection(const cbor::Value& section_value) {
    // |section_value| of index section must be a map.
    if (!section_value.is_map()) {
      RunErrorCallbackAndDestroy("Index section must be a map.");
      return false;
    }

    base::flat_map<GURL, mojom::BundleResponseLocationPtr> requests;

    auto responses_section = section_offsets_.find(kResponsesSection);
    DCHECK(responses_section != section_offsets_.end());
    const uint64_t responses_section_offset = responses_section->second.first;
    const uint64_t responses_section_length = responses_section->second.second;

    // For each (url, responses) entry in the index map.
    for (const auto& item : section_value.GetMap()) {
      if (!item.first.is_string()) {
        RunErrorCallbackAndDestroy("Index section: key must be a string.");
        return false;
      }
      if (!item.second.is_array()) {
        RunErrorCallbackAndDestroy("Index section: value must be an array.");
        return false;
      }
      const std::string& url = item.first.GetString();
      const cbor::Value::ArrayValue& responses_array = item.second.GetArray();

      GURL parsed_url = ParseExchangeURL(url, base_url_);

      if (!parsed_url.is_valid()) {
        std::string message = base::StringPrintf(
            "Index section: exchange URL \"%s\" is not valid.", url.c_str());
        if (base_url_.is_empty())
          message += " (Relative URLs are not allowed in this context.)";
        RunErrorCallbackAndDestroy(message);
        return false;
      }

      if (responses_array.size() != 2) {
        RunErrorCallbackAndDestroy(
            "Index section: the size of a response array per URL should be "
            "exactly 2.");
        return false;
      }
      if (!responses_array[0].is_unsigned() ||
          !responses_array[1].is_unsigned()) {
        RunErrorCallbackAndDestroy(
            "Index section: offset and length values must be unsigned.");
        return false;
      }
      uint64_t offset = responses_array[0].GetUnsigned();
      uint64_t length = responses_array[1].GetUnsigned();

      uint64_t response_end;
      if (!base::CheckAdd(offset, length).AssignIfValid(&response_end) ||
          response_end > responses_section_length) {
        RunErrorCallbackAndDestroy("Index section: response out of range.");
        return false;
      }
      uint64_t offset_within_stream = responses_section_offset + offset;

      requests.insert(std::make_pair(
          parsed_url,
          mojom::BundleResponseLocation::New(offset_within_stream, length)));
    }

    metadata_->requests = std::move(requests);
    return true;
  }

  // https://github.com/WICG/webpackage/blob/main/extensions/signatures-section.md
  // signatures = [
  //   authorities: [*authority],
  //   vouched-subsets: [*{
  //     authority: index-in-authorities,
  //     sig: bstr,
  //     signed: bstr  ; Expected to hold a signed-subset item.
  //   }],
  // ]
  // authority = augmented-certificate
  // index-in-authorities = uint
  //
  // signed-subset = {
  //   validity-url: whatwg-url,
  //   auth-sha256: bstr,
  //   date: uint,
  //   expires: uint,
  //   subset-hashes: {+
  //     whatwg-url => [variants-value, +resource-integrity]
  //   },
  //   * tstr => any,
  // }
  // resource-integrity = (
  //   header-sha256: bstr,
  //   payload-integrity-header: tstr
  // )
  bool ParseSignaturesSection(const cbor::Value& section_value) {
    if (!section_value.is_array() || section_value.GetArray().size() != 2) {
      RunErrorCallbackAndDestroy(
          "Signatures section must be an array of size 2.");
      return false;
    }
    const cbor::Value& authorities_value = section_value.GetArray()[0];
    const cbor::Value& vouched_subsets_value = section_value.GetArray()[1];

    if (!authorities_value.is_array()) {
      RunErrorCallbackAndDestroy("Authorities must be an array.");
      return false;
    }
    std::vector<mojom::AugmentedCertificatePtr> authorities;
    for (const cbor::Value& value : authorities_value.GetArray()) {
      // ParseAugmentedCertificate deletes |this| on failure.
      auto authority = ParseAugmentedCertificate(value);
      if (!authority)
        return false;
      authorities.push_back(std::move(authority));
    }

    if (!vouched_subsets_value.is_array()) {
      RunErrorCallbackAndDestroy("Vouched-subsets must be an array.");
      return false;
    }
    std::vector<mojom::VouchedSubsetPtr> vouched_subsets;
    for (const cbor::Value& value : vouched_subsets_value.GetArray()) {
      if (!value.is_map()) {
        RunErrorCallbackAndDestroy(
            "An element of vouched-subsets must be a map.");
        return false;
      }
      const cbor::Value::MapValue& item_map = value.GetMap();

      auto* authority_value = Lookup(item_map, "authority");
      if (!authority_value || !authority_value->is_unsigned()) {
        RunErrorCallbackAndDestroy(
            "authority is not found in vouched-subsets map, or not an "
            "unsigned.");
        return false;
      }
      int64_t authority = authority_value->GetUnsigned();

      auto* sig_value = Lookup(item_map, "sig");
      if (!sig_value || !sig_value->is_bytestring()) {
        RunErrorCallbackAndDestroy(
            "sig is not found in vouched-subsets map, or not a bytestring.");
        return false;
      }
      const cbor::Value::BinaryValue& sig = sig_value->GetBytestring();

      auto* signed_value = Lookup(item_map, "signed");
      if (!signed_value || !signed_value->is_bytestring()) {
        RunErrorCallbackAndDestroy(
            "signed is not found in vouched-subsets map, or not a bytestring.");
        return false;
      }
      const cbor::Value::BinaryValue& raw_signed =
          signed_value->GetBytestring();

      // ParseSignedSubset deletes |this| on failure.
      auto parsed_signed = ParseSignedSubset(raw_signed);
      if (!parsed_signed)
        return false;

      vouched_subsets.push_back(mojom::VouchedSubset::New(
          authority, sig, raw_signed, std::move(parsed_signed)));
    }

    metadata_->authorities = std::move(authorities);

    metadata_->vouched_subsets = std::move(vouched_subsets);

    return true;
  }

  // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#critical-section
  //   critical = [*tstr]
  bool ParseCriticalSection(const cbor::Value& section_value) {
    if (!section_value.is_array()) {
      RunErrorCallbackAndDestroy("Critical section must be an array.");
      return false;
    }
    // "If the client has not implemented a section named by one of the items in
    // this list, the client MUST fail to parse the bundle as a whole."
    for (const cbor::Value& elem : section_value.GetArray()) {
      if (!elem.is_string()) {
        RunErrorCallbackAndDestroy(
            "Non-string element in the critical section.");
        return false;
      }
      const auto& section_name = elem.GetString();
      if (!IsMetadataSection(section_name) &&
          section_name != kResponsesSection) {
        RunErrorCallbackAndDestroy("Unknown critical section.");
        return false;
      }
    }
    return true;
  }

  // https://github.com/WICG/webpackage/blob/main/extensions/primary-section.md
  //  primary = whatwg-url
  bool ParsePrimarySection(const cbor::Value& section_value) {
    if (!section_value.is_string()) {
      RunErrorCallbackAndDestroy("Primary section must be a string.");
      return false;
    }

    GURL parsed_url = ParseExchangeURL(section_value.GetString(), base_url_);

    if (!parsed_url.is_valid()) {
      RunErrorCallbackAndDestroy("Primary URL is not a valid exchange URL.");
      return false;
    }
    metadata_->primary_url = std::move(parsed_url);
    return true;
  }

  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cert-chain-format
  mojom::AugmentedCertificatePtr ParseAugmentedCertificate(
      const cbor::Value& value) {
    if (!value.is_map()) {
      RunErrorCallbackAndDestroy("augmented-certificate must be a map.");
      return nullptr;
    }
    const cbor::Value::MapValue& item_map = value.GetMap();
    mojom::AugmentedCertificatePtr authority =
        mojom::AugmentedCertificate::New();

    auto* cert_value = Lookup(item_map, "cert");
    if (!cert_value || !cert_value->is_bytestring()) {
      RunErrorCallbackAndDestroy(
          "cert is not found in augmented-certificate, or not a bytestring.");
      return nullptr;
    }
    authority->cert = cert_value->GetBytestring();

    if (auto* ocsp_value = Lookup(item_map, "ocsp")) {
      if (!ocsp_value->is_bytestring()) {
        RunErrorCallbackAndDestroy("ocsp is not a bytestring.");
        return nullptr;
      }
      authority->ocsp = ocsp_value->GetBytestring();
    }

    if (auto* sct_value = Lookup(item_map, "sct")) {
      if (!sct_value->is_bytestring()) {
        RunErrorCallbackAndDestroy("sct is not a bytestring.");
        return nullptr;
      }
      authority->sct = sct_value->GetBytestring();
    }
    return authority;
  }

  // https://github.com/WICG/webpackage/blob/main/extensions/signatures-section.md
  mojom::SignedSubsetPtr ParseSignedSubset(
      const cbor::Value::BinaryValue& signed_bytes) {
    // Parse |signed_bytes| as a CBOR item.
    cbor::Reader::DecoderError error;
    absl::optional<cbor::Value> value =
        cbor::Reader::Read(signed_bytes, &error);
    if (!value) {
      RunErrorCallbackAndDestroy(
          std::string("Error parsing signed bytes as CBOR: ") +
          cbor::Reader::ErrorCodeToString(error));
      return nullptr;
    }

    if (!value->is_map()) {
      RunErrorCallbackAndDestroy("signed-subset must be a CBOR map");
      return nullptr;
    }
    const cbor::Value::MapValue& value_map = value->GetMap();

    auto* validity_url_value = Lookup(value_map, "validity-url");
    if (!validity_url_value || !validity_url_value->is_string()) {
      RunErrorCallbackAndDestroy(
          "validity-url is not found in signed-subset, or not a string.");
      return nullptr;
    }
    // TODO(crbug.com/966753): Revisit this once requirements for validity URL
    // are speced.
    // TODO(crbug.com/1247939): Consider supporting relative validity URL.
    GURL validity_url(validity_url_value->GetString());
    if (!validity_url.is_valid()) {
      RunErrorCallbackAndDestroy("Cannot parse validity-url.");
      return nullptr;
    }

    auto* auth_sha256_value = Lookup(value_map, "auth-sha256");
    if (!auth_sha256_value || !auth_sha256_value->is_bytestring()) {
      RunErrorCallbackAndDestroy(
          "auth-sha256 is not found in signed-subset, or not a bytestring.");
      return nullptr;
    }
    auto auth_sha256 = auth_sha256_value->GetBytestring();

    auto* date_value = Lookup(value_map, "date");
    if (!date_value || !date_value->is_unsigned()) {
      RunErrorCallbackAndDestroy(
          "date is not found in signed-subset, or not an unsigned.");
      return nullptr;
    }
    auto date = date_value->GetUnsigned();

    auto* expires_value = Lookup(value_map, "expires");
    if (!expires_value || !expires_value->is_unsigned()) {
      RunErrorCallbackAndDestroy(
          "expires is not found in signed-subset, or not an unsigned.");
      return nullptr;
    }
    auto expires = expires_value->GetUnsigned();

    auto* subset_hashes_value = Lookup(value_map, "subset-hashes");
    if (!subset_hashes_value || !subset_hashes_value->is_map()) {
      RunErrorCallbackAndDestroy(
          "subset-hashes is not found in signed-subset, or not a map.");
      return nullptr;
    }
    base::flat_map<GURL, mojom::SubsetHashesValuePtr> subset_hashes;

    for (const auto& item : subset_hashes_value->GetMap()) {
      if (!item.first.is_string()) {
        RunErrorCallbackAndDestroy("subset-hashes: key must be a string.");
        return nullptr;
      }
      if (!item.second.is_array()) {
        RunErrorCallbackAndDestroy("subset-hashes: value must be an array.");
        return nullptr;
      }
      const std::string& url = item.first.GetString();
      const cbor::Value::ArrayValue& value_array = item.second.GetArray();

      // TODO(crbug.com/1247939): Consider supporting relative URL in the
      // signature section.
      GURL parsed_url = ParseExchangeURL(url, /*base_url=*/GURL());
      if (!parsed_url.is_valid()) {
        RunErrorCallbackAndDestroy("subset-hashes: exchange URL is not valid.");
        return nullptr;
      }

      if (value_array.empty() || !value_array[0].is_bytestring()) {
        RunErrorCallbackAndDestroy(
            "subset-hashes: the first element of array must be a bytestring.");
        return nullptr;
      }
      base::StringPiece variants_value = value_array[0].GetBytestringAsString();
      if (value_array.size() < 3 || value_array.size() % 2 != 1) {
        RunErrorCallbackAndDestroy(
            "subset-hashes: unexpected size of value array.");
        return nullptr;
      }
      std::vector<mojom::ResourceIntegrityPtr> resource_integrities;
      for (size_t i = 1; i < value_array.size(); i += 2) {
        if (!value_array[i].is_bytestring()) {
          RunErrorCallbackAndDestroy(
              "subset-hashes: header-sha256 must be a byte string.");
          return nullptr;
        }
        if (!value_array[i + 1].is_string()) {
          RunErrorCallbackAndDestroy(
              "subset-hashes: payload-integrity-header must be a string.");
          return nullptr;
        }
        resource_integrities.push_back(mojom::ResourceIntegrity::New(
            value_array[i].GetBytestring(), value_array[i + 1].GetString()));
      }
      subset_hashes.insert(std::make_pair(
          parsed_url,
          mojom::SubsetHashesValue::New(std::string(variants_value),
                                        std::move(resource_integrities))));
    }

    return mojom::SignedSubset::New(validity_url, auth_sha256, date, expires,
                                    std::move(subset_hashes));
  }

  // Returns nullptr if |key| is not in |map|.
  const cbor::Value* Lookup(const cbor::Value::MapValue& map, const char* key) {
    auto iter = map.find(cbor::Value(key));
    if (iter == map.end())
      return nullptr;
    return &iter->second;
  }

  void RunSuccessCallbackAndDestroy() {
    std::move(callback_).Run(std::move(metadata_), nullptr);
    delete this;
  }

  void RunErrorCallbackAndDestroy(
      const std::string& message,
      mojom::BundleParseErrorType error_type =
          mojom::BundleParseErrorType::kFormatError) {
    DLOG(ERROR) << "Parsing web bundle error: " << message;
    mojom::BundleMetadataParseErrorPtr err =
        mojom::BundleMetadataParseError::New(error_type, message);
    std::move(callback_).Run(nullptr, std::move(err));
    delete this;
  }

  // Implements SharedBundleDataSource::Observer.
  void OnDisconnect() override {
    RunErrorCallbackAndDestroy(
        "Data source disconnected.",
        mojom::BundleParseErrorType::kParserInternalError);
  }

  scoped_refptr<SharedBundleDataSource> data_source_;
  const GURL base_url_;
  ParseMetadataCallback callback_;
  SectionOffsets section_offsets_;
  mojom::BundleMetadataPtr metadata_;
  base::WeakPtrFactory<MetadataParser> weak_factory_{this};
};

// A parser for reading single item from the responses section. This class owns
// itself and will self destruct after calling the ParseResponseCallback.
class WebBundleParser::ResponseParser
    : public WebBundleParser::SharedBundleDataSource::Observer {
 public:
  ResponseParser(scoped_refptr<SharedBundleDataSource> data_source,
                 uint64_t response_offset,
                 uint64_t response_length,
                 WebBundleParser::ParseResponseCallback callback)
      : data_source_(data_source),
        response_offset_(response_offset),
        response_length_(response_length),
        callback_(std::move(callback)) {
    data_source_->AddObserver(this);
  }

  ResponseParser(const ResponseParser&) = delete;
  ResponseParser& operator=(const ResponseParser&) = delete;

  ~ResponseParser() override { data_source_->RemoveObserver(this); }

  void Start(uint64_t buffer_size = kInitialBufferSizeForResponse) {
    const uint64_t length = std::min(response_length_, buffer_size);
    data_source_->Read(response_offset_, length,
                       base::BindOnce(&ResponseParser::ParseResponseHeader,
                                      weak_factory_.GetWeakPtr(), length));
  }

 private:
  // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-responses
  //   responses = [*response]
  //   response = [headers: bstr .cbor headers, payload: bstr]
  //   headers = {* bstr => bstr}
  void ParseResponseHeader(uint64_t expected_data_length,
                           const absl::optional<std::vector<uint8_t>>& data) {
    if (!data || data->size() != expected_data_length) {
      RunErrorCallbackAndDestroy("Error reading response header.");
      return;
    }
    InputReader input(*data);

    // |response| must be an array of length 2 (headers and payload).
    auto num_elements = input.ReadCBORHeader(CBORType::kArray);
    if (!num_elements || *num_elements != 2) {
      RunErrorCallbackAndDestroy("Array size of response must be 2.");
      return;
    }

    auto header_length = input.ReadCBORHeader(CBORType::kByteString);
    if (!header_length) {
      RunErrorCallbackAndDestroy("Cannot parse response header length.");
      return;
    }

    // "The length of the headers byte string in a response MUST be less than
    // 524288 (512*1024) bytes, and recipients MUST fail to load a response with
    // longer headers"
    if (*header_length >= kMaxResponseHeaderLength) {
      RunErrorCallbackAndDestroy("Response header is too big.");
      return;
    }

    // If we don't have enough data for the headers and the CBOR header of the
    // payload, re-read with a larger buffer size.
    const uint64_t required_buffer_size = std::min(
        input.CurrentOffset() + *header_length + kMaxCBORItemHeaderSize,
        response_length_);
    if (data->size() < required_buffer_size) {
      DVLOG(1) << "Re-reading response header with a buffer of size "
               << required_buffer_size;
      Start(required_buffer_size);
      return;
    }

    // Parse headers.
    auto headers_bytes = input.ReadBytes(*header_length);
    if (!headers_bytes) {
      RunErrorCallbackAndDestroy("Cannot read response headers.");
      return;
    }
    cbor::Reader::DecoderError error;
    absl::optional<cbor::Value> headers_value =
        cbor::Reader::Read(*headers_bytes, &error);
    if (!headers_value) {
      RunErrorCallbackAndDestroy("Cannot parse response headers.");
      return;
    }

    auto parsed_headers = ConvertCBORValueToHeaders(*headers_value);
    if (!parsed_headers) {
      RunErrorCallbackAndDestroy("Cannot parse response headers.");
      return;
    }

    // "Each response's headers MUST include a :status pseudo-header with
    // exactly 3 ASCII decimal digits and MUST NOT include any other
    // pseudo-headers."
    const auto pseudo_status = parsed_headers->pseudos.find(":status");
    if (parsed_headers->pseudos.size() != 1 ||
        pseudo_status == parsed_headers->pseudos.end()) {
      RunErrorCallbackAndDestroy(
          "Response headers map must have exactly one pseudo-header, :status.");
      return;
    }
    int status;
    const auto& status_str = pseudo_status->second;
    if (status_str.size() != 3 ||
        !base::ranges::all_of(status_str, base::IsAsciiDigit<char>) ||
        !base::StringToInt(status_str, &status)) {
      RunErrorCallbackAndDestroy(":status must be 3 ASCII decimal digits.");
      return;
    }

    // Parse payload.
    auto payload_length = input.ReadCBORHeader(CBORType::kByteString);
    if (!payload_length) {
      RunErrorCallbackAndDestroy("Cannot parse response payload length.");
      return;
    }

    // "If a response's payload is not empty, its headers MUST include a
    // Content-Type header (Section 8.3 of [I-D.ietf-httpbis-semantics])."
    if (*payload_length > 0 &&
        !parsed_headers->headers.contains("content-type")) {
      RunErrorCallbackAndDestroy(
          "Non-empty response must have a content-type header.");
      return;
    }

    if (input.CurrentOffset() + *payload_length != response_length_) {
      RunErrorCallbackAndDestroy("Unexpected payload length.");
      return;
    }

    mojom::BundleResponsePtr response = mojom::BundleResponse::New();
    response->response_code = status;
    response->response_headers = std::move(parsed_headers->headers);
    response->payload_offset = response_offset_ + input.CurrentOffset();
    response->payload_length = *payload_length;
    RunSuccessCallbackAndDestroy(std::move(response));
  }

  void RunSuccessCallbackAndDestroy(mojom::BundleResponsePtr response) {
    std::move(callback_).Run(std::move(response), nullptr);
    delete this;
  }

  void RunErrorCallbackAndDestroy(
      const std::string& message,
      mojom::BundleParseErrorType error_type =
          mojom::BundleParseErrorType::kFormatError) {
    std::move(callback_).Run(
        nullptr, mojom::BundleResponseParseError::New(error_type, message));
    delete this;
  }

  // Implements SharedBundleDataSource::Observer.
  void OnDisconnect() override {
    RunErrorCallbackAndDestroy(
        "Data source disconnected.",
        mojom::BundleParseErrorType::kParserInternalError);
  }

  scoped_refptr<SharedBundleDataSource> data_source_;
  uint64_t response_offset_;
  uint64_t response_length_;
  ParseResponseCallback callback_;

  base::WeakPtrFactory<ResponseParser> weak_factory_{this};
};

WebBundleParser::SharedBundleDataSource::SharedBundleDataSource(
    mojo::PendingRemote<mojom::BundleDataSource> pending_data_source)
    : data_source_(std::move(pending_data_source)) {
  data_source_.set_disconnect_handler(base::BindOnce(
      &SharedBundleDataSource::OnDisconnect, base::Unretained(this)));
}

void WebBundleParser::SharedBundleDataSource::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WebBundleParser::SharedBundleDataSource::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

WebBundleParser::SharedBundleDataSource::~SharedBundleDataSource() = default;

void WebBundleParser::SharedBundleDataSource::OnDisconnect() {
  // |observer->OnDisconnect()| below may remove the last external reference to
  // |this|.
  scoped_refptr<SharedBundleDataSource> keep_alive(this);
  for (Observer& observer : observers_)
    observer.OnDisconnect();
}

void WebBundleParser::SharedBundleDataSource::Read(
    uint64_t offset,
    uint64_t length,
    mojom::BundleDataSource::ReadCallback callback) {
  data_source_->Read(offset, length, std::move(callback));
}

void WebBundleParser::SharedBundleDataSource::Length(
    mojom::BundleDataSource::LengthCallback callback) {
  data_source_->Length(std::move(callback));
}

void WebBundleParser::SharedBundleDataSource::IsRandomAccessContext(
    mojom::BundleDataSource::IsRandomAccessContextCallback callback) {
  data_source_->IsRandomAccessContext(std::move(callback));
}

WebBundleParser::WebBundleParser(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver,
    mojo::PendingRemote<mojom::BundleDataSource> data_source,
    const GURL& base_url)
    : receiver_(this, std::move(receiver)),
      data_source_(
          base::MakeRefCounted<SharedBundleDataSource>(std::move(data_source))),
      base_url_(base_url) {
  DCHECK(base_url_.is_empty() || base_url_.is_valid());
  receiver_.set_disconnect_handler(base::BindOnce(
      &base::DeletePointer<WebBundleParser>, base::Unretained(this)));
}

WebBundleParser::~WebBundleParser() = default;

void WebBundleParser::ParseIntegrityBlock(
    ParseIntegrityBlockCallback callback) {
  IntegrityBlockParser* parser =
      new IntegrityBlockParser(data_source_, std::move(callback));
  parser->Start();
}

void WebBundleParser::ParseMetadata(int64_t offset,
                                    ParseMetadataCallback callback) {
  MetadataParser* parser =
      new MetadataParser(data_source_, base_url_, std::move(callback));
  if (offset >= 0) {
    parser->StartAtOffset(offset);
  } else {
    DCHECK_EQ(offset, -1);
    // If no offset is specified, then where we start parsing the web bundle
    // metadata depends on whether or not it is loaded in a random-access
    // context. If random-access into the web bundle is possible, then we use
    // the `length` field at its end to determine the start of the web bundle.
    // If random-access into the web bundle is not possible, then we simply
    // start at the top.
    parser->Start();
  }
}

void WebBundleParser::ParseResponse(uint64_t response_offset,
                                    uint64_t response_length,
                                    ParseResponseCallback callback) {
  ResponseParser* parser = new ResponseParser(
      data_source_, response_offset, response_length, std::move(callback));
  parser->Start();
}

}  // namespace web_package
