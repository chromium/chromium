// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_parser.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/cbor/reader.h"
#include "components/web_package/input_reader.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/integrity_block_parser.h"
#include "components/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_util.h"

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

// A list of (section-name, length) pairs.
using SectionLengths = std::vector<std::pair<std::string, uint64_t>>;

// A map from section name to (offset, length) pair.
using SectionOffsets = std::map<std::string, std::pair<uint64_t, uint64_t>>;

bool IsMetadataSection(const std::string& name) {
  return (name == kCriticalSection || name == kIndexSection ||
          name == kPrimarySection);
}

// Parses a `section-lengths` CBOR item.
// https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-bundle-sections
//   section-lengths = [* (section-name: tstr, length: uint) ]
std::optional<SectionLengths> ParseSectionLengths(
    base::span<const uint8_t> data) {
  cbor::Reader::DecoderError error;
  std::optional<cbor::Value> value = cbor::Reader::Read(data, &error);
  if (!value.has_value() || !value->is_array()) {
    return std::nullopt;
  }

  const cbor::Value::ArrayValue& array = value->GetArray();
  if (array.size() % 2 != 0) {
    return std::nullopt;
  }

  SectionLengths result;
  for (size_t i = 0; i < array.size(); i += 2) {
    if (!array[i].is_string() || !array[i + 1].is_unsigned()) {
      return std::nullopt;
    }
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
std::optional<ParsedHeaders> ConvertCBORValueToHeaders(
    const cbor::Value& headers_value) {
  // |headers_value| of headers must be a map.
  if (!headers_value.is_map()) {
    return std::nullopt;
  }

  ParsedHeaders result;

  for (const auto& item : headers_value.GetMap()) {
    if (!item.first.is_bytestring() || !item.second.is_bytestring()) {
      return std::nullopt;
    }
    std::string_view name = item.first.GetBytestringAsString();
    std::string_view value = item.second.GetBytestringAsString();

    // If name contains any upper-case or non-ASCII characters, return an error.
    // This matches the requirement in Section 8.1.2 of [RFC7540].
    if (!base::IsStringASCII(name) ||
        base::ranges::any_of(name, base::IsAsciiUpper<char>)) {
      return std::nullopt;
    }

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
        !net::HttpUtil::IsValidHeaderValue(value)) {
      return std::nullopt;
    }

    // headers[name] must not exist, because CBOR maps cannot contain duplicate
    // keys. This is ensured by cbor::Reader.
    DCHECK(!result.headers.contains(name));

    result.headers.insert(
        std::make_pair(std::string(name), std::string(value)));
  }

  return result;
}

GURL ParseExchangeURL(std::string_view str, const GURL& base_url) {
  DCHECK(base_url.is_empty() || base_url.is_valid());

  if (!base::IsStringUTF8(str)) {
    return GURL();
  }

  GURL url = base_url.is_valid() ? base_url.Resolve(str) : GURL(str);
  if (!url.is_valid()) {
    return GURL();
  }

  // Exchange URL must not have a fragment or credentials.
  if (url.has_ref() || url.has_username() || url.has_password()) {
    return GURL();
  }

  return url;
}

}  // namespace

// A parser for bundle's metadata.
class WebBundleParser::MetadataParser
    : public WebBundleParser::WebBundleSectionParser {
 public:
  MetadataParser(mojo::Remote<mojom::BundleDataSource>& data_source
                     LIFETIME_BOUND,
                 GURL base_url,
                 std::optional<uint64_t> offset,
                 ParseMetadataCallback callback)
      : data_source_(data_source),
        base_url_(std::move(base_url)),
        start_reading_offset_(std::move(offset)),
        result_callback_(std::move(callback)) {
    DCHECK(base_url_.is_empty() || base_url_.is_valid());
  }

  MetadataParser(const MetadataParser&) = delete;
  MetadataParser& operator=(const MetadataParser&) = delete;

  ~MetadataParser() override {
    if (!complete_callback_.is_null()) {
      RunErrorCallback("Data source disconnected.",
                       mojom::BundleParseErrorType::kParserInternalError);
    }
  }

  // Starts parsing of the web bundle. If the data source is backed by a
  // random-access, read the trailing `length` field at the end of the web
  // bundle file and start from that offset.
  // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-trailing-length
  // If offset is provided, it starts parsing of the web bundle at the specified
  // offset, ignoring the `length` field of the web bundle.
  void StartParsing(
      WebBundleParser::WebBundleSectionParser::ParsingCompleteCallback callback)
      override {
    CHECK(!result_callback_.is_null());
    complete_callback_ = std::move(callback);

    // If no offset is specified, then where we start parsing the Web Bundle
    // metadata depends on whether or not it is loaded in a random-access
    // context. If random-access into the Web Bundle is possible, then we use
    // the `length` field at its end to determine the start of the Web Bundle.
    // If random-access into the Web Bundle is not possible, then we simply
    // start at the top.
    if (start_reading_offset_.has_value()) {
      ReadMagicBytes(start_reading_offset_.value());
    } else {
      data_source_->get()->IsRandomAccessContext(
          base::BindOnce(&MetadataParser::OnIsRandomAccessContext,
                         weak_factory_.GetWeakPtr()));
    }
  }

 private:
  void OnIsRandomAccessContext(const bool is_random_access_context) {
    if (!is_random_access_context) {
      // If the data source is not backed by a random-access context, assume
      // that the web bundle starts at the very first byte of the file and
      // ignore the trailing length field of the bundle.
      ReadMagicBytes(0);
    } else {
      // Otherwise read the length of the file (not the web bundle).
      data_source_->get()->Length(base::BindOnce(
          &MetadataParser::OnFileLengthRead, weak_factory_.GetWeakPtr()));
    }
  }
  void OnFileLengthRead(const int64_t file_length) {
    if (file_length < 0) {
      RunErrorCallback("Error reading bundle length.");
      return;
    }
    if (static_cast<uint64_t>(file_length) < kTrailingLengthNumBytes) {
      RunErrorCallback("Error reading bundle length.");
      return;
    }

    // Read the last 8 bytes of the file that correspond to the trailing length
    // field of the web bundle.
    data_source_->get()->Read(
        file_length - kTrailingLengthNumBytes, kTrailingLengthNumBytes,
        base::BindOnce(&MetadataParser::ParseWebBundleLength,
                       weak_factory_.GetWeakPtr(), file_length));
  }

  void ParseWebBundleLength(const uint64_t file_length,
                            const std::optional<std::vector<uint8_t>>& data) {
    if (!data.has_value()) {
      RunErrorCallback("Error reading bundle length.");
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
      RunErrorCallback("Error reading bundle length.");
      return;
    }

    if (web_bundle_length > file_length) {
      RunErrorCallback("Invalid bundle length.");
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
    data_source_->get()->Read(
        offset_in_stream, length,
        base::BindOnce(&MetadataParser::ParseMagicBytes,
                       weak_factory_.GetWeakPtr(), offset_in_stream));
  }

  // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-top-level-structure
  void ParseMagicBytes(uint64_t offset_in_stream,
                       const std::optional<std::vector<uint8_t>>& data) {
    if (!data) {
      RunErrorCallback("Error reading bundle magic bytes.");
      return;
    }

    InputReader input(*data);

    // Read the first byte denoting a CBOR array size. It must be equal to 0x85
    // (5).
    const auto array_size = input.ReadByte();
    if (!array_size) {
      RunErrorCallback("Missing CBOR array size byte.");
      return;
    }

    // Let kBundleB1HeadByte pass this check, to report custom error message for
    // b1 bundles.
    if (*array_size != kBundleHeadByte && *array_size != kBundleB1HeadByte) {
      RunErrorCallback("Wrong magic bytes.");
      return;
    }

    // Check the magic bytes "48 F0 9F 8C 90 F0 9F 93 A6".
    const auto magic = input.ReadBytes(sizeof(kBundleMagicBytes));
    if (!magic || !base::ranges::equal(*magic, kBundleMagicBytes)) {
      RunErrorCallback("Wrong magic bytes.");
      return;
    }

    // Let version be the result of reading 5 bytes from stream.
    const auto version = input.ReadBytes(sizeof(kVersionB2MagicBytes));
    if (!version) {
      RunErrorCallback("Cannot read version bytes.");
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
      RunErrorCallback(message, mojom::BundleParseErrorType::kVersionError);
      return;
    }
    if (*array_size != kBundleHeadByte) {
      RunErrorCallback("Wrong CBOR array size of the top-level structure");
      return;
    }

    const auto section_lengths_length =
        input.ReadCBORHeader(CBORType::kByteString);
    if (!section_lengths_length) {
      RunErrorCallback("Cannot parse the size of section-lengths.");
      return;
    }

    // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-top-level-structure
    // "The section-lengths array is embedded in a byte string to facilitate
    // reading it from a network. This byte string MUST be less than 8192
    // (8*1024) bytes long, and parsers MUST NOT load any data from a
    // section-lengths item longer than this."
    if (*section_lengths_length >= kMaxSectionLengthsCBORSize) {
      RunErrorCallback(
          "The section-lengths CBOR must be smaller than 8192 bytes.");
      return;
    }

    // In the next step, we will parse the content of `section-lengths`,
    // and the CBOR header of `sections`.
    const uint64_t length = *section_lengths_length + kMaxCBORItemHeaderSize;

    offset_in_stream += input.CurrentOffset();
    data_source_->get()->Read(
        offset_in_stream, length,
        base::BindOnce(&MetadataParser::ParseBundleHeader,
                       weak_factory_.GetWeakPtr(), offset_in_stream,
                       *section_lengths_length));
  }

  void ParseBundleHeader(uint64_t offset_in_stream,
                         uint64_t section_lengths_length,
                         const std::optional<std::vector<uint8_t>>& data) {
    if (!data) {
      RunErrorCallback("Error reading bundle header.");
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
      RunErrorCallback("Cannot read section-lengths.");
      return;
    }
    // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-bundle-sections
    //   section-lengths = [* (section-name: tstr, length: uint) ]
    const auto section_lengths = ParseSectionLengths(*section_lengths_bytes);
    if (!section_lengths) {
      RunErrorCallback("Cannot parse section-lengths.");
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
      RunErrorCallback("Cannot parse the number of sections.");
      return;
    }

    // "The sections array contains the sections' content. The length of this
    // array MUST be exactly half the length of the section-lengths array, and
    // parsers MUST NOT load any data if that is not the case."
    if (*num_sections != section_lengths->size()) {
      RunErrorCallback("Unexpected number of sections.");
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
        RunErrorCallback("Duplicated section.");
        return;
      }

      if (!base::CheckAdd(current_offset, length)
               .AssignIfValid(&current_offset)) {
        RunErrorCallback("Integer overflow calculating section offsets.");
        return;
      }
    }

    // "The "responses" section MUST appear after the other three sections
    // defined here, and parsers MUST NOT load any data if that is not the
    // case."
    if (section_lengths->empty() ||
        section_lengths->back().first != kResponsesSection) {
      RunErrorCallback("Responses section is not the last in section-lengths.");
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
      if (!IsMetadataSection(name)) {
        continue;
      }
      const uint64_t section_offset = section_iter->second.first;
      const uint64_t section_length = section_iter->second.second;
      if (section_length > kMaxMetadataSectionSize) {
        RunErrorCallback(
            "Metadata sections larger than 1MB are not supported.");
        return;
      }

      data_source_->get()->Read(
          section_offset, section_length,
          base::BindOnce(&MetadataParser::ParseMetadataSection,
                         weak_factory_.GetWeakPtr(), section_iter,
                         section_length));
      // This loop will be resumed by ParseMetadataSection().
      return;
    }

    // The bundle MUST contain the "index" and "responses" sections.
    if (metadata_->requests.empty()) {
      RunErrorCallback("Bundle must have an index section.");
      return;
    }

    RunSuccessCallback();
  }

  void ParseMetadataSection(SectionOffsets::const_iterator section_iter,
                            uint64_t expected_data_length,
                            const std::optional<std::vector<uint8_t>>& data) {
    if (!data || data->size() != expected_data_length) {
      RunErrorCallback("Error reading section content.");
      return;
    }

    // Parse the section contents as a CBOR item.
    cbor::Reader::DecoderError error;
    std::optional<cbor::Value> section_value =
        cbor::Reader::Read(*data, &error);
    if (!section_value) {
      RunErrorCallback(std::string("Error parsing section contents as CBOR: ") +
                       cbor::Reader::ErrorCodeToString(error));
      return;
    }

    const auto& name = section_iter->first;
    // Note: Parse*Section() delete |this| on failure.
    if (name == kIndexSection) {
      if (!ParseIndexSection(*section_value)) {
        return;
      }
    } else if (name == kCriticalSection) {
      if (!ParseCriticalSection(*section_value)) {
        return;
      }
    } else if (name == kPrimarySection) {
      if (!ParsePrimarySection(*section_value)) {
        return;
      }
    } else {
      NOTREACHED_IN_MIGRATION();
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
      RunErrorCallback("Index section must be a map.");
      return false;
    }

    base::flat_map<GURL, mojom::BundleResponseLocationPtr> requests;

    auto responses_section = section_offsets_.find(kResponsesSection);
    CHECK(responses_section != section_offsets_.end(),
          base::NotFatalUntil::M130);
    const uint64_t responses_section_offset = responses_section->second.first;
    const uint64_t responses_section_length = responses_section->second.second;

    // For each (url, responses) entry in the index map.
    for (const auto& item : section_value.GetMap()) {
      if (!item.first.is_string()) {
        RunErrorCallback("Index section: key must be a string.");
        return false;
      }
      if (!item.second.is_array()) {
        RunErrorCallback("Index section: value must be an array.");
        return false;
      }
      const std::string& url = item.first.GetString();
      const cbor::Value::ArrayValue& responses_array = item.second.GetArray();

      GURL parsed_url = ParseExchangeURL(url, base_url_);

      if (!parsed_url.is_valid()) {
        std::string message = base::StringPrintf(
            "Index section: exchange URL \"%s\" is not valid.", url.c_str());
        if (base_url_.is_empty()) {
          message += " (Relative URLs are not allowed in this context.)";
        }
        RunErrorCallback(message);
        return false;
      }

      if (responses_array.size() != 2) {
        RunErrorCallback(
            "Index section: the size of a response array per URL should be "
            "exactly 2.");
        return false;
      }
      if (!responses_array[0].is_unsigned() ||
          !responses_array[1].is_unsigned()) {
        RunErrorCallback(
            "Index section: offset and length values must be unsigned.");
        return false;
      }
      uint64_t offset = responses_array[0].GetUnsigned();
      uint64_t length = responses_array[1].GetUnsigned();

      uint64_t response_end;
      if (!base::CheckAdd(offset, length).AssignIfValid(&response_end) ||
          response_end > responses_section_length) {
        RunErrorCallback("Index section: response out of range.");
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

  // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#critical-section
  //   critical = [*tstr]
  bool ParseCriticalSection(const cbor::Value& section_value) {
    if (!section_value.is_array()) {
      RunErrorCallback("Critical section must be an array.");
      return false;
    }
    // "If the client has not implemented a section named by one of the items in
    // this list, the client MUST fail to parse the bundle as a whole."
    for (const cbor::Value& elem : section_value.GetArray()) {
      if (!elem.is_string()) {
        RunErrorCallback("Non-string element in the critical section.");
        return false;
      }
      const auto& section_name = elem.GetString();
      if (!IsMetadataSection(section_name) &&
          section_name != kResponsesSection) {
        RunErrorCallback("Unknown critical section.");
        return false;
      }
    }
    return true;
  }

  // https://github.com/WICG/webpackage/blob/main/extensions/primary-section.md
  //  primary = whatwg-url
  bool ParsePrimarySection(const cbor::Value& section_value) {
    if (!section_value.is_string()) {
      RunErrorCallback("Primary section must be a string.");
      return false;
    }

    GURL parsed_url = ParseExchangeURL(section_value.GetString(), base_url_);

    if (!parsed_url.is_valid()) {
      RunErrorCallback("Primary URL is not a valid exchange URL.");
      return false;
    }
    metadata_->primary_url = std::move(parsed_url);
    return true;
  }

  void RunSuccessCallback() {
    std::move(complete_callback_)
        .Run(base::BindOnce(std::move(result_callback_), std::move(metadata_),
                            nullptr));
  }

  void RunErrorCallback(const std::string& message,
                        mojom::BundleParseErrorType error_type =
                            mojom::BundleParseErrorType::kFormatError) {
    DLOG(ERROR) << "Parsing web bundle error: " << message;
    mojom::BundleMetadataParseErrorPtr err =
        mojom::BundleMetadataParseError::New(error_type, message);
    std::move(complete_callback_)
        .Run(base::BindOnce(std::move(result_callback_), nullptr,
                            std::move(err)));
  }

  const raw_ref<mojo::Remote<mojom::BundleDataSource>> data_source_;
  const GURL base_url_;
  std::optional<uint64_t> start_reading_offset_;
  ParseMetadataCallback result_callback_;
  ParsingCompleteCallback complete_callback_;
  SectionOffsets section_offsets_;
  mojom::BundleMetadataPtr metadata_;
  base::WeakPtrFactory<MetadataParser> weak_factory_{this};
};

// A parser for reading single item from the responses section.
class WebBundleParser::ResponseParser
    : public WebBundleParser::WebBundleSectionParser {
 public:
  ResponseParser(mojo::Remote<mojom::BundleDataSource>& data_source
                     LIFETIME_BOUND,
                 uint64_t response_offset,
                 uint64_t response_length,
                 WebBundleParser::ParseResponseCallback callback)
      : data_source_(data_source),
        response_offset_(response_offset),
        response_length_(response_length),
        result_callback_(std::move(callback)) {}

  ResponseParser(const ResponseParser&) = delete;
  ResponseParser& operator=(const ResponseParser&) = delete;

  ~ResponseParser() override {
    if (!complete_callback_.is_null()) {
      RunErrorCallback("Data source disconnected.",
                       mojom::BundleParseErrorType::kParserInternalError);
    }
  }

  void StartParsing(
      WebBundleParser::WebBundleSectionParser::ParsingCompleteCallback callback)
      override {
    CHECK(!result_callback_.is_null());
    complete_callback_ = std::move(callback);
    StartWithBufferSize(kInitialBufferSizeForResponse);
  }

 private:
  void StartWithBufferSize(uint64_t buffer_size) {
    const uint64_t length = std::min(response_length_, buffer_size);
    data_source_->get()->Read(
        response_offset_, length,
        base::BindOnce(&ResponseParser::ParseResponseHeader,
                       weak_factory_.GetWeakPtr(), length));
  }
  // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-responses
  //   responses = [*response]
  //   response = [headers: bstr .cbor headers, payload: bstr]
  //   headers = {* bstr => bstr}
  void ParseResponseHeader(uint64_t expected_data_length,
                           const std::optional<std::vector<uint8_t>>& data) {
    if (!data || data->size() != expected_data_length) {
      RunErrorCallback("Error reading response header.");
      return;
    }
    InputReader input(*data);

    // |response| must be an array of length 2 (headers and payload).
    auto num_elements = input.ReadCBORHeader(CBORType::kArray);
    if (!num_elements || *num_elements != 2) {
      RunErrorCallback("Array size of response must be 2.");
      return;
    }

    auto header_length = input.ReadCBORHeader(CBORType::kByteString);
    if (!header_length) {
      RunErrorCallback("Cannot parse response header length.");
      return;
    }

    // "The length of the headers byte string in a response MUST be less than
    // 524288 (512*1024) bytes, and recipients MUST fail to load a response with
    // longer headers"
    if (*header_length >= kMaxResponseHeaderLength) {
      RunErrorCallback("Response header is too big.");
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
      StartWithBufferSize(required_buffer_size);
      return;
    }

    // Parse headers.
    auto headers_bytes = input.ReadBytes(*header_length);
    if (!headers_bytes) {
      RunErrorCallback("Cannot read response headers.");
      return;
    }
    cbor::Reader::DecoderError error;
    std::optional<cbor::Value> headers_value =
        cbor::Reader::Read(*headers_bytes, &error);
    if (!headers_value) {
      RunErrorCallback("Cannot parse response headers.");
      return;
    }

    auto parsed_headers = ConvertCBORValueToHeaders(*headers_value);
    if (!parsed_headers) {
      RunErrorCallback("Cannot parse response headers.");
      return;
    }

    // "Each response's headers MUST include a :status pseudo-header with
    // exactly 3 ASCII decimal digits and MUST NOT include any other
    // pseudo-headers."
    const auto pseudo_status = parsed_headers->pseudos.find(":status");
    if (parsed_headers->pseudos.size() != 1 ||
        pseudo_status == parsed_headers->pseudos.end()) {
      RunErrorCallback(
          "Response headers map must have exactly one pseudo-header, :status.");
      return;
    }
    int status;
    const auto& status_str = pseudo_status->second;
    if (status_str.size() != 3 ||
        !base::ranges::all_of(status_str, base::IsAsciiDigit<char>) ||
        !base::StringToInt(status_str, &status)) {
      RunErrorCallback(":status must be 3 ASCII decimal digits.");
      return;
    }

    // Parse payload.
    auto payload_length = input.ReadCBORHeader(CBORType::kByteString);
    if (!payload_length) {
      RunErrorCallback("Cannot parse response payload length.");
      return;
    }

    // "If a response's payload is not empty, its headers MUST include a
    // Content-Type header (Section 8.3 of [I-D.ietf-httpbis-semantics])."
    if (*payload_length > 0 &&
        !parsed_headers->headers.contains("content-type")) {
      RunErrorCallback("Non-empty response must have a content-type header.");
      return;
    }

    if (input.CurrentOffset() + *payload_length != response_length_) {
      RunErrorCallback("Unexpected payload length.");
      return;
    }

    mojom::BundleResponsePtr response = mojom::BundleResponse::New();
    response->response_code = status;
    response->response_headers = std::move(parsed_headers->headers);
    response->payload_offset = response_offset_ + input.CurrentOffset();
    response->payload_length = *payload_length;
    RunSuccessCallback(std::move(response));
  }

  void RunSuccessCallback(mojom::BundleResponsePtr response) {
    std::move(complete_callback_)
        .Run(base::BindOnce(std::move(result_callback_), std::move(response),
                            nullptr));
  }

  void RunErrorCallback(const std::string& message,
                        mojom::BundleParseErrorType error_type =
                            mojom::BundleParseErrorType::kFormatError) {
    std::move(complete_callback_)
        .Run(base::BindOnce(
            std::move(result_callback_), nullptr,
            mojom::BundleResponseParseError::New(error_type, message)));
  }

  const raw_ref<mojo::Remote<mojom::BundleDataSource>> data_source_;
  uint64_t response_offset_;
  uint64_t response_length_;
  ParseResponseCallback result_callback_;
  WebBundleParser::WebBundleSectionParser::ParsingCompleteCallback
      complete_callback_;

  base::WeakPtrFactory<ResponseParser> weak_factory_{this};
};

WebBundleParser::WebBundleParser(
    mojo::PendingRemote<mojom::BundleDataSource> data_source,
    GURL base_url)
    : base_url_(std::move(base_url)), data_source_(std::move(data_source)) {
  data_source_.set_disconnect_handler(
      base::BindOnce(&WebBundleParser::OnDisconnect, base::Unretained(this)));
  DCHECK(base_url_.is_empty() || base_url_.is_valid());
}

WebBundleParser::~WebBundleParser() {
  // Explicitly delete active parsers to avoid potential problems
  // with deletion of them in |active_parsers_|'s dtor and consequently
  // referring to |active_parsers_| in OnParsingComplete().
  //
  // Avoid using container clear method directly on the member variable
  // since parser destructor can call back to this class OnParsingComplete
  // method via the complete_callback_. OnParsingComplete would in such
  // case call erase method on the same container trying to remove an object
  // from whose destructor it has been called. C++ and //base containers
  // generally don't support re-entrancy so this would result in undefined
  // behavior.
  auto parsers = std::exchange(active_parsers_, {});
  parsers.clear();
}

void WebBundleParser::ParseIntegrityBlock(
    ParseIntegrityBlockCallback callback) {
  if (CheckIfClosed()) {
    return;
  }

  std::unique_ptr<WebBundleSectionParser> parser =
      std::make_unique<web_package::IntegrityBlockParser>(*data_source_,
                                                          std::move(callback));
  ActivateParser(std::move(parser));
}

void WebBundleParser::ParseMetadata(std::optional<uint64_t> offset,
                                    ParseMetadataCallback callback) {
  if (CheckIfClosed()) {
    return;
  }

  std::unique_ptr<WebBundleSectionParser> parser =
      std::make_unique<MetadataParser>(data_source_, base_url_,
                                       std::move(offset), std::move(callback));
  ActivateParser(std::move(parser));
}

void WebBundleParser::ParseResponse(uint64_t response_offset,
                                    uint64_t response_length,
                                    ParseResponseCallback callback) {
  if (CheckIfClosed()) {
    return;
  }

  std::unique_ptr<WebBundleSectionParser> parser =
      std::make_unique<ResponseParser>(data_source_, response_offset,
                                       response_length, std::move(callback));
  ActivateParser(std::move(parser));
}

void WebBundleParser::ActivateParser(
    std::unique_ptr<WebBundleSectionParser> parser) {
  auto* parser_ptr = parser.get();
  active_parsers_.insert(std::move(parser));
  parser_ptr->StartParsing(base::BindOnce(&WebBundleParser::OnParsingComplete,
                                          base::Unretained(this), parser_ptr));
}

void WebBundleParser::OnParsingComplete(WebBundleSectionParser* parser,
                                        base::OnceClosure result_callback) {
  std::move(result_callback).Run();
  active_parsers_.erase(parser);
}

void WebBundleParser::OnDisconnect() {
  active_parsers_.clear();
}

void WebBundleParser::Close(CloseCallback parser_closed_callback) {
  is_closed_ = true;
  active_parsers_.clear();
  data_source_->Close(base::BindOnce(&WebBundleParser::OnDataSourceClosed,
                                     base::Unretained(this),
                                     std::move(parser_closed_callback)));
}

void WebBundleParser::OnDataSourceClosed(CloseCallback parser_closed_callback) {
  std::move(parser_closed_callback).Run();
}

bool WebBundleParser::CheckIfClosed() {
  if (is_closed_) {
    mojo::ReportBadMessage("Attempt to access the closed web bundle parser");
  }
  return is_closed_;
}

}  // namespace web_package
