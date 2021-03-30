// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_parser.h"

#include <algorithm>

#include "base/big_endian.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/cbor/reader.h"
#include "components/web_package/web_bundle_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_util.h"
#include "url/url_constants.h"

namespace web_package {

namespace {

// The maximum length of the CBOR item header (type and argument).
// https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#parse-type-argument
constexpr uint64_t kMaxCBORItemHeaderSize = 9;

// The maximum size of the section-lengths CBOR item.
constexpr uint64_t kMaxSectionLengthsCBORSize = 8192;

// The maximum size of a metadata section allowed in this implementation.
constexpr uint64_t kMaxMetadataSectionSize = 1 * 1024 * 1024;

// The maximum size of the response header CBOR.
constexpr uint64_t kMaxResponseHeaderLength = 512 * 1024;

// The initial buffer size for reading an item from the response section.
constexpr uint64_t kInitialBufferSizeForResponse = 4096;

// Step 2. of
// https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#load-metadata
const uint8_t kBundleMagicBytes[] = {
    0x86, 0x48, 0xF0, 0x9F, 0x8C, 0x90, 0xF0, 0x9F, 0x93, 0xA6,
};

// Step 7. of
// https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#load-metadata
// We use an implementation-specific version string "b1\0\0".
const uint8_t kVersionB1MagicBytes[] = {
    0x44, 0x62, 0x31, 0x00, 0x00,
};

// Section names.
constexpr char kCriticalSection[] = "critical";
constexpr char kIndexSection[] = "index";
constexpr char kManifestSection[] = "manifest";
constexpr char kResponsesSection[] = "responses";
constexpr char kSignaturesSection[] = "signatures";

// https://tools.ietf.org/html/draft-ietf-cbor-7049bis-05#section-3.1
enum class CBORType {
  kByteString = 2,
  kTextString = 3,
  kArray = 4,
};

// A list of (section-name, length) pairs.
using SectionLengths = std::vector<std::pair<std::string, uint64_t>>;

// A map from section name to (offset, length) pair.
using SectionOffsets = std::map<std::string, std::pair<uint64_t, uint64_t>>;

bool IsMetadataSection(const std::string& name) {
  return (name == kCriticalSection || name == kIndexSection ||
          name == kManifestSection || name == kSignaturesSection);
}

// Parses a `section-lengths` CBOR item.
// https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#load-metadata
base::Optional<SectionLengths> ParseSectionLengths(
    base::span<const uint8_t> data) {
  cbor::Reader::DecoderError error;
  base::Optional<cbor::Value> value = cbor::Reader::Read(data, &error);
  if (!value.has_value() || !value->is_array())
    return base::nullopt;

  const cbor::Value::ArrayValue& array = value->GetArray();
  if (array.size() % 2 != 0)
    return base::nullopt;

  SectionLengths result;
  for (size_t i = 0; i < array.size(); i += 2) {
    if (!array[i].is_string() || !array[i + 1].is_unsigned())
      return base::nullopt;
    result.emplace_back(array[i].GetString(), array[i + 1].GetUnsigned());
  }
  return result;
}

struct ParsedHeaders {
  base::flat_map<std::string, std::string> headers;
  base::flat_map<std::string, std::string> pseudos;
};

// https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#cbor-headers
base::Optional<ParsedHeaders> ConvertCBORValueToHeaders(
    const cbor::Value& headers_value) {
  // Step 1. If item doesn’t match the headers rule in the above CDDL, return
  // an error.
  if (!headers_value.is_map())
    return base::nullopt;

  // Step 2. Let headers be a new header list ([FETCH]).
  // Step 3. Let pseudos be an empty map ([INFRA]).
  ParsedHeaders result;

  // Step 4. For each pair (name, value) in item:
  for (const auto& item : headers_value.GetMap()) {
    if (!item.first.is_bytestring() || !item.second.is_bytestring())
      return base::nullopt;
    base::StringPiece name = item.first.GetBytestringAsString();
    base::StringPiece value = item.second.GetBytestringAsString();

    // Step 4.1. If name contains any upper-case or non-ASCII characters, return
    // an error. This matches the requirement in Section 8.1.2 of [RFC7540].
    if (!base::IsStringASCII(name) ||
        std::any_of(name.begin(), name.end(), base::IsAsciiUpper<char>))
      return base::nullopt;

    // Step 4.2. If name starts with a ‘:’:
    if (!name.empty() && name[0] == ':') {
      // Step 4.2.1. Assert: pseudos[name] does not exist, because CBOR maps
      // cannot contain duplicate keys.
      // This is ensured by cbor::Reader.
      DCHECK(!result.pseudos.contains(name));
      // Step 4.2.2. Set pseudos[name] to value.
      result.pseudos.insert(
          std::make_pair(name.as_string(), value.as_string()));
      // Step 4.3.3. Continue.
      continue;
    }

    // Step 4.3. If name or value doesn’t satisfy the requirements for a header
    // in [FETCH], return an error.
    if (!net::HttpUtil::IsValidHeaderName(name) ||
        !net::HttpUtil::IsValidHeaderValue(value))
      return base::nullopt;

    // Step 4.4. Assert: headers does not contain ([FETCH]) name, because CBOR
    // maps cannot contain duplicate keys and an earlier step rejected
    // upper-case bytes.
    // This is ensured by cbor::Reader.
    DCHECK(!result.headers.contains(name));

    // Step 4.5. Append (name, value) to headers.
    result.headers.insert(std::make_pair(name.as_string(), value.as_string()));
  }

  // Step 5. Return (headers, pseudos).
  return result;
}

// A utility class for reading various values from input buffer.
class InputReader {
 public:
  explicit InputReader(base::span<const uint8_t> buf) : buf_(buf) {}

  uint64_t CurrentOffset() const { return current_offset_; }
  size_t Size() const { return buf_.size(); }

  base::Optional<uint8_t> ReadByte() {
    if (buf_.empty())
      return base::nullopt;
    uint8_t byte = buf_[0];
    Advance(1);
    return byte;
  }

  template <typename T>
  bool ReadBigEndian(T* out) {
    auto bytes = ReadBytes(sizeof(T));
    if (!bytes)
      return false;
    base::ReadBigEndian(reinterpret_cast<const char*>(bytes->data()), out);
    return true;
  }

  base::Optional<base::span<const uint8_t>> ReadBytes(size_t n) {
    if (buf_.size() < n)
      return base::nullopt;
    auto result = buf_.subspan(0, n);
    Advance(n);
    return result;
  }

  base::Optional<base::StringPiece> ReadString(size_t n) {
    auto bytes = ReadBytes(n);
    if (!bytes)
      return base::nullopt;
    base::StringPiece str(reinterpret_cast<const char*>(bytes->data()),
                          bytes->size());
    if (!base::IsStringUTF8(str))
      return base::nullopt;
    return str;
  }

  // Parses the type and argument of a CBOR item from the input head. If parsed
  // successfully and the type matches |expected_type|, returns the argument.
  // Otherwise returns nullopt.
  base::Optional<uint64_t> ReadCBORHeader(CBORType expected_type) {
    auto pair = ReadTypeAndArgument();
    if (!pair || pair->first != expected_type)
      return base::nullopt;
    return pair->second;
  }

 private:
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#parse-type-argument
  base::Optional<std::pair<CBORType, uint64_t>> ReadTypeAndArgument() {
    base::Optional<uint8_t> first_byte = ReadByte();
    if (!first_byte)
      return base::nullopt;

    CBORType type = static_cast<CBORType>((*first_byte & 0xE0) / 0x20);
    uint8_t b = *first_byte & 0x1F;

    if (b <= 23)
      return std::make_pair(type, b);
    if (b == 24) {
      auto content = ReadByte();
      if (!content || *content < 24)
        return base::nullopt;
      return std::make_pair(type, *content);
    }
    if (b == 25) {
      uint16_t content;
      if (!ReadBigEndian(&content) || content >> 8 == 0)
        return base::nullopt;
      return std::make_pair(type, content);
    }
    if (b == 26) {
      uint32_t content;
      if (!ReadBigEndian(&content) || content >> 16 == 0)
        return base::nullopt;
      return std::make_pair(type, content);
    }
    if (b == 27) {
      uint64_t content;
      if (!ReadBigEndian(&content) || content >> 32 == 0)
        return base::nullopt;
      return std::make_pair(type, content);
    }
    return base::nullopt;
  }

  void Advance(size_t n) {
    DCHECK_LE(n, buf_.size());
    buf_ = buf_.subspan(n);
    current_offset_ += n;
  }

  base::span<const uint8_t> buf_;
  uint64_t current_offset_ = 0;

  DISALLOW_COPY_AND_ASSIGN(InputReader);
};

GURL ParseExchangeURL(base::StringPiece str) {
  if (!base::IsStringUTF8(str))
    return GURL();

  GURL url(str);
  if (!url.is_valid())
    return GURL();

  // Exchange URL must not have a fragment or credentials.
  if (url.has_ref() || url.has_username() || url.has_password())
    return GURL();

  // For now, we allow only http:, https: and urn:uuid URLs in Web Bundles.
  // TODO(crbug.com/966753): Revisit this once
  // https://github.com/WICG/webpackage/issues/468 is resolved.
  if (!url.SchemeIsHTTPOrHTTPS() && !IsValidUrnUuidURL(url))
    return GURL();

  return url;
}

}  // namespace

class WebBundleParser::SharedBundleDataSource::Observer {
 public:
  Observer() {}
  virtual ~Observer() {}
  virtual void OnDisconnect() = 0;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

// A parser for bundle's metadata. This class owns itself and will self destruct
// after calling the ParseMetadataCallback.
class WebBundleParser::MetadataParser
    : WebBundleParser::SharedBundleDataSource::Observer {
 public:
  MetadataParser(scoped_refptr<SharedBundleDataSource> data_source,
                 ParseMetadataCallback callback)
      : data_source_(data_source), callback_(std::move(callback)) {
    data_source_->AddObserver(this);
  }
  ~MetadataParser() override { data_source_->RemoveObserver(this); }

  void Start() {
    // First, we will parse `magic`, `version`, and the CBOR header of
    // `primary-url`.
    // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#top-level
    const uint64_t length = sizeof(kBundleMagicBytes) +
                            sizeof(kVersionB1MagicBytes) +
                            kMaxCBORItemHeaderSize;
    data_source_->Read(0, length,
                       base::BindOnce(&MetadataParser::ParseMagicBytes,
                                      weak_factory_.GetWeakPtr()));
  }

 private:
  // Step 1-4 of
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#load-metadata
  void ParseMagicBytes(const base::Optional<std::vector<uint8_t>>& data) {
    if (!data) {
      RunErrorCallbackAndDestroy("Error reading bundle magic bytes.");
      return;
    }

    // Step 1. "Seek to offset 0 in stream. Assert: this operation doesn't
    // fail."
    InputReader input(*data);

    // Step 2. "If reading 10 bytes from stream returns an error or doesn't
    // return the bytes with hex encoding "86 48 F0 9F 8C 90 F0 9F 93 A6"
    // (the CBOR encoding of the 6-item array initial byte and 8-byte bytestring
    // initial byte, followed by 🌐📦 in UTF-8), return a "format error"."
    const auto magic = input.ReadBytes(sizeof(kBundleMagicBytes));
    if (!magic ||
        !std::equal(magic->begin(), magic->end(), std::begin(kBundleMagicBytes),
                    std::end(kBundleMagicBytes))) {
      RunErrorCallbackAndDestroy("Wrong magic bytes.");
      return;
    }

    // Step 3. "Let version be the result of reading 5 bytes from stream. If
    // this is an error, return a "format error"."
    const auto version = input.ReadBytes(sizeof(kVersionB1MagicBytes));
    if (!version) {
      RunErrorCallbackAndDestroy("Cannot read version bytes.");
      return;
    }
    if (!std::equal(version->begin(), version->end(),
                    std::begin(kVersionB1MagicBytes),
                    std::end(kVersionB1MagicBytes))) {
      version_mismatch_ = true;
      // Continue parsing until Step 7 where we get a fallback URL, and
      // then return "version error" with the fallback URL.
    }

    // Step 4. "Let urlType and urlLength be the result of reading the type and
    // argument of a CBOR item from stream (Section 3.5.3). If this is an error
    // or urlType is not 3 (a CBOR text string), return a "format error"."
    const auto url_length = input.ReadCBORHeader(CBORType::kTextString);
    if (!url_length) {
      RunErrorCallbackAndDestroy("Cannot parse the size of fallback URL.");
      return;
    }

    // In the next step, we will parse the content of `primary-url`,
    // `section-lengths`, and the CBOR header of `sections`.
    const uint64_t length =
        *url_length + kMaxSectionLengthsCBORSize + kMaxCBORItemHeaderSize * 2;
    data_source_->Read(input.CurrentOffset(), length,
                       base::BindOnce(&MetadataParser::ParseBundleHeader,
                                      weak_factory_.GetWeakPtr(), *url_length,
                                      input.CurrentOffset()));
  }

  // Step 5-21 of
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#load-metadata
  void ParseBundleHeader(uint64_t url_length,
                         uint64_t offset_in_stream,
                         const base::Optional<std::vector<uint8_t>>& data) {
    if (!data) {
      RunErrorCallbackAndDestroy("Error reading bundle header.");
      return;
    }
    InputReader input(*data);

    // Step 5. "Let fallbackUrlBytes be the result of reading urlLength bytes
    // from stream. If this is an error, return a "format error"."
    const auto fallback_url_string = input.ReadString(url_length);
    if (!fallback_url_string) {
      RunErrorCallbackAndDestroy("Cannot read fallback URL.");
      return;
    }

    // Step 6. "Let fallbackUrl be the result of parsing ([URL]) the UTF-8
    // decoding of fallbackUrlBytes with no base URL. If either the UTF-8
    // decoding or parsing fails, return a "format error"."
    // For now, we enforce the same restriction as exchages' request URL.
    // TODO(crbug.com/966753): Revisit URL requirements here once
    // https://github.com/WICG/webpackage/issues/469 is resolved.
    GURL fallback_url = ParseExchangeURL(*fallback_url_string);
    if (!fallback_url.is_valid()) {
      RunErrorCallbackAndDestroy("Cannot parse fallback URL.");
      return;
    }

    // "Note: From this point forward, errors also include the fallback URL to
    // help clients recover."
    fallback_url_ = std::move(fallback_url);

    // Step 7. "If version does not have the hex encoding "44 31 00 00 00" (the
    // CBOR encoding of a 4-byte byte string holding an ASCII "1" followed by
    // three 0 bytes), return a "version error" with fallbackUrl. "
    // Note: We use an implementation-specific version string
    // kVersionB1MagicBytes.
    if (version_mismatch_) {
      RunErrorCallbackAndDestroy(
          "Version error: this implementation only supports "
          "bundle format of version b1.",
          mojom::BundleParseErrorType::kVersionError);
      return;
    }

    // Step 8. "Let sectionLengthsLength be the result of getting the length of
    // the CBOR bytestring header from stream (Section 3.5.2). If this is an
    // error, return a "format error" with fallbackUrl."
    const auto section_lengths_length =
        input.ReadCBORHeader(CBORType::kByteString);
    if (!section_lengths_length) {
      RunErrorCallbackAndDestroy("Cannot parse the size of section-lengths.");
      return;
    }
    // Step 9. "If sectionLengthsLength is 8192 (8*1024) or greater, return a
    // "format error" with fallbackUrl."
    if (*section_lengths_length >= kMaxSectionLengthsCBORSize) {
      RunErrorCallbackAndDestroy(
          "The section-lengths CBOR must be smaller than 8192 bytes.");
      return;
    }

    // Step 10. "Let sectionLengthsBytes be the result of reading
    // sectionLengthsLength bytes from stream. If sectionLengthsBytes is an
    // error, return a "format error" with fallbackUrl."
    const auto section_lengths_bytes = input.ReadBytes(*section_lengths_length);
    if (!section_lengths_bytes) {
      RunErrorCallbackAndDestroy("Cannot read section-lengths.");
      return;
    }

    // Step 11. "Let sectionLengths be the result of parsing one CBOR item
    // (Section 3.5) from sectionLengthsBytes, matching the section-lengths
    // rule in the CDDL ([I-D.ietf-cbor-cddl]) above. If sectionLengths is an
    // error, return a "format error" with fallbackUrl."
    const auto section_lengths = ParseSectionLengths(*section_lengths_bytes);
    if (!section_lengths) {
      RunErrorCallbackAndDestroy("Cannot parse section-lengths.");
      return;
    }

    // Step 12. "Let (sectionsType, numSections) be the result of parsing the
    // type and argument of a CBOR item from stream (Section 3.5.3)."
    const auto num_sections = input.ReadCBORHeader(CBORType::kArray);
    if (!num_sections) {
      RunErrorCallbackAndDestroy("Cannot parse the number of sections.");
      return;
    }

    // Step 13. "If sectionsType is not 4 (a CBOR array) or numSections is not
    // half of the length of sectionLengths, return a "format error" with
    // fallbackUrl."
    if (*num_sections != section_lengths->size()) {
      RunErrorCallbackAndDestroy("Unexpected number of sections.");
      return;
    }

    // Step 14. "Let sectionsStart be the current offset within stream."
    // Note: This doesn't exceed |size_|.
    const uint64_t sections_start = offset_in_stream + input.CurrentOffset();

    // Step 15. "Let knownSections be the subset of the Section 6.2 that this
    // client has implemented."
    // Step 16. "Let ignoredSections be an empty set."

    // This implementation doesn't use knownSections nor ignoredSections.

    // Step 17. "Let sectionOffsets be an empty map ([INFRA]) from section names
    // to (offset, length) pairs. These offsets are relative to the start of
    // stream."

    // |section_offsets_| is defined as a class member field.

    // Step 18. "Let currentOffset be sectionsStart."
    uint64_t current_offset = sections_start;

    // Step 19. "For each ("name", length) pair of adjacent elements in
    // sectionLengths:"
    for (const auto& pair : *section_lengths) {
      const std::string& name = pair.first;
      const uint64_t length = pair.second;
      // Step 19.1. "If "name"'s specification in knownSections says not to
      // process other sections, add those sections' names to ignoredSections."

      // There're no such sections at the moment.

      // Step 19.2. "If sectionOffsets["name"] exists, return a "format error"
      // with fallbackUrl. That is, duplicate sections are forbidden."
      // Step 19.3. "Set sectionOffsets["name"] to (currentOffset, length)."
      bool added = section_offsets_
                       .insert(std::make_pair(
                           name, std::make_pair(current_offset, length)))
                       .second;
      if (!added) {
        RunErrorCallbackAndDestroy("Duplicated section.");
        return;
      }

      // Step 19.4. "Set currentOffset to currentOffset + length."
      if (!base::CheckAdd(current_offset, length)
               .AssignIfValid(&current_offset)) {
        RunErrorCallbackAndDestroy(
            "Integer overflow calculating section offsets.");
        return;
      }
    }

    // Step 20. "If the "responses" section is not last in sectionLengths,
    // return a "format error" with fallbackUrl. This allows a streaming parser
    // to assume that it'll know the requests by the time their responses
    // arrive."
    if (section_lengths->empty() ||
        section_lengths->back().first != kResponsesSection) {
      RunErrorCallbackAndDestroy(
          "Responses section is not the last in section-lengths.");
      return;
    }

    // Step 21. "Let metadata be a map ([INFRA]) initially containing the single
    // key/value pair "primaryUrl"/fallbackUrl."
    metadata_ = mojom::BundleMetadata::New();
    metadata_->primary_url = fallback_url_;

    ReadMetadataSections(section_offsets_.begin());
  }

  // Step 22-25 of
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#load-metadata
  void ReadMetadataSections(SectionOffsets::const_iterator section_iter) {
    // Step 22. "For each "name" -> (offset, length) triple in sectionOffsets:"
    for (; section_iter != section_offsets_.end(); ++section_iter) {
      const auto& name = section_iter->first;
      // Step 22.1. "If "name" isn't in knownSections, continue to the next
      // triple."
      // Step 22.2. "If "name"'s Metadata field (Section 6.2) is "No", continue
      // to the next triple."
      if (!IsMetadataSection(name))
        continue;

      // Step 22.3. "If "name" is in ignoredSections, continue to the next
      // triple."
      // In the current spec, ignoredSections is always empty.

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

    // Step 23. "Assert: metadata has an entry with the key "primaryUrl"."
    DCHECK(!metadata_->primary_url.is_empty());

    // Step 24. "If metadata doesn't have an entry with the key "requests",
    // return a "format error" with fallbackUrl."
    if (metadata_->requests.empty()) {
      RunErrorCallbackAndDestroy("Bundle must have an index section.");
      return;
    }

    // Step 25. "Return metadata."
    RunSuccessCallbackAndDestroy();
  }

  void ParseMetadataSection(SectionOffsets::const_iterator section_iter,
                            uint64_t expected_data_length,
                            const base::Optional<std::vector<uint8_t>>& data) {
    if (!data || data->size() != expected_data_length) {
      RunErrorCallbackAndDestroy("Error reading section content.");
      return;
    }

    // Parse the section contents as a CBOR item.
    cbor::Reader::DecoderError error;
    base::Optional<cbor::Value> section_value =
        cbor::Reader::Read(*data, &error);
    if (!section_value) {
      RunErrorCallbackAndDestroy(
          std::string("Error parsing section contents as CBOR: ") +
          cbor::Reader::ErrorCodeToString(error));
      return;
    }

    // Step 22.6. "Follow "name"'s specification from knownSections to process
    // the section, passing sectionContents, stream, sectionOffsets, and
    // metadata. If this returns an error, return a "format error" with
    // fallbackUrl."
    const auto& name = section_iter->first;
    // Note: Parse*Section() delete |this| on failure.
    if (name == kIndexSection) {
      if (!ParseIndexSection(*section_value))
        return;
    } else if (name == kManifestSection) {
      if (!ParseManifestSection(*section_value))
        return;
    } else if (name == kSignaturesSection) {
      if (!ParseSignaturesSection(*section_value))
        return;
    } else if (name == kCriticalSection) {
      if (!ParseCriticalSection(*section_value))
        return;
    } else {
      NOTREACHED();
    }
    // Resume the loop of Step 22.
    ReadMetadataSections(++section_iter);
  }

  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#index-section
  bool ParseIndexSection(const cbor::Value& section_value) {
    // Step 1. "Let index be the result of parsing sectionContents as a CBOR
    // item matching the index rule in the above CDDL (Section 3.5). If index is
    // an error, return an error."
    if (!section_value.is_map()) {
      RunErrorCallbackAndDestroy("Index section must be a map.");
      return false;
    }

    // Step 2. "Let requests be an initially-empty map ([INFRA]) from URLs to
    // response descriptions, each of which is either a single
    // location-in-stream value or a pair of a Variants header field value
    // ([I-D.ietf-httpbis-variants]) and a map from that value's possible
    // Variant-Keys to location-in-stream values, as described in Section 2.2."
    base::flat_map<GURL, mojom::BundleIndexValuePtr> requests;

    // Step 3. "Let MakeRelativeToStream be a function that takes a
    // location-in-responses value (offset, length) and returns a
    // ResponseMetadata struct or error by running the following
    // sub-steps:"
    // The logic of MakeRelativeToStream is inlined at the callsite below.
    auto responses_section = section_offsets_.find(kResponsesSection);
    DCHECK(responses_section != section_offsets_.end());
    const uint64_t responses_section_offset = responses_section->second.first;
    const uint64_t responses_section_length = responses_section->second.second;

    // Step 4. "For each (url, responses) entry in the index map:"
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

      // Step 4.1. "Let parsedUrl be the result of parsing ([URL]) url with no
      // base URL."
      GURL parsed_url = ParseExchangeURL(url);

      // Step 4.2. "If parsedUrl is a failure, its fragment is not null, or it
      // includes credentials, return an error."
      if (!parsed_url.is_valid()) {
        RunErrorCallbackAndDestroy("Index section: exchange URL is not valid.");
        return false;
      }

      // Step 4.3. "If the first element of responses is the empty string:"
      if (responses_array.empty() || !responses_array[0].is_bytestring()) {
        RunErrorCallbackAndDestroy(
            "Index section: the first element of responses array must be a "
            "bytestring.");
        return false;
      }
      base::StringPiece variants_value =
          responses_array[0].GetBytestringAsString();
      if (variants_value.empty()) {
        // Step 4.3.1. "If the length of responses is not 3 (i.e. there is more
        // than one location-in-responses in responses), return an error."
        if (responses_array.size() != 3) {
          RunErrorCallbackAndDestroy(
              "Index section: unexpected size of responses array.");
          return false;
        }
      } else {
        // Step 4.4. "Otherwise:"
        // TODO(crbug.com/969596): Parse variants_value to compute the number of
        // variantKeys, and check that responses_array has
        // (2 * #variantKeys + 1) elements.
        if (responses_array.size() < 3 || responses_array.size() % 2 != 1) {
          RunErrorCallbackAndDestroy(
              "Index section: unexpected size of responses array.");
          return false;
        }
      }
      // Instead of constructing a map from Variant-Keys to location-in-stream,
      // this implementation just returns the responses array's structure as
      // a BundleIndexValue.
      std::vector<mojom::BundleResponseLocationPtr> response_locations;
      for (size_t i = 1; i < responses_array.size(); i += 2) {
        if (!responses_array[i].is_unsigned() ||
            !responses_array[i + 1].is_unsigned()) {
          RunErrorCallbackAndDestroy(
              "Index section: offset and length values must be unsigned.");
          return false;
        }
        uint64_t offset = responses_array[i].GetUnsigned();
        uint64_t length = responses_array[i + 1].GetUnsigned();

        // MakeRelativeToStream (Step 3.) is inlined here.
        // Step 3.1. "If offset + length is larger than
        // sectionOffsets["responses"].length, return an error."
        uint64_t response_end;
        if (!base::CheckAdd(offset, length).AssignIfValid(&response_end) ||
            response_end > responses_section_length) {
          RunErrorCallbackAndDestroy("Index section: response out of range.");
          return false;
        }
        // Step 3.2. "Otherwise, return a ResponseMetadata struct whose offset
        // is sectionOffsets["responses"].offset + offset and whose length is
        // length."

        // This doesn't wrap because (offset <= responses_section_length) and
        // (responses_section_offset + responses_section_length) doesn't wrap.
        uint64_t offset_within_stream = responses_section_offset + offset;

        response_locations.push_back(
            mojom::BundleResponseLocation::New(offset_within_stream, length));
      }
      requests.insert(std::make_pair(
          parsed_url,
          mojom::BundleIndexValue::New(variants_value.as_string(),
                                       std::move(response_locations))));
    }

    // Step 5. "Set metadata["requests"] to requests."
    metadata_->requests = std::move(requests);
    return true;
  }

  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#manifest-section
  bool ParseManifestSection(const cbor::Value& section_value) {
    // Step 1. "Let urlString be the result of parsing sectionContents as a CBOR
    // item matching the above manifest rule (Section 3.5). If urlString is an
    // error, return that error."
    if (!section_value.is_string()) {
      RunErrorCallbackAndDestroy("Manifest section must be a string.");
      return false;
    }
    // Step 2. "Let url be the result of parsing ([URL]) urlString with no base
    // URL."
    GURL parsed_url = ParseExchangeURL(section_value.GetString());

    // Step 3. "If url is a failure, its fragment is not null, or it includes
    // credentials, return an error."
    if (!parsed_url.is_valid()) {
      RunErrorCallbackAndDestroy("Manifest URL is not a valid exchange URL.");
      return false;
    }
    // Step 4. "Set metadata["manifest"] to url."
    metadata_->manifest_url = std::move(parsed_url);
    return true;
  }

  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#signatures-section
  bool ParseSignaturesSection(const cbor::Value& section_value) {
    // Step 1. "Let signatures be the result of parsing sectionContents as a
    // CBOR item matching the signatures rule in the above CDDL (Section 3.5)."
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

    // Step 2. "Set metadata["authorities"] to the list of authorities in the
    // first element of the signatures array."
    metadata_->authorities = std::move(authorities);

    // Step 3. "Set metadata["vouched-subsets"] to the second element of the
    // signatures array."
    metadata_->vouched_subsets = std::move(vouched_subsets);

    return true;
  }

  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#critical-section
  bool ParseCriticalSection(const cbor::Value& section_value) {
    // Step 1. "Let critical be the result of parsing sectionContents as a CBOR
    // item matching the above critical rule (Section 3.5). If critical is an
    // error, return that error."
    if (!section_value.is_array()) {
      RunErrorCallbackAndDestroy("Critical section must be an array.");
      return false;
    }
    // Step 2. "For each value sectionName in the critical list, if the client
    // has not implemented sections named sectionName, return an error."
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

  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#signatures-section
  mojom::SignedSubsetPtr ParseSignedSubset(
      const cbor::Value::BinaryValue& signed_bytes) {
    // Parse |signed_bytes| as a CBOR item.
    cbor::Reader::DecoderError error;
    base::Optional<cbor::Value> value =
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

      GURL parsed_url = ParseExchangeURL(url);
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
          mojom::SubsetHashesValue::New(variants_value.as_string(),
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
    mojom::BundleMetadataParseErrorPtr err =
        mojom::BundleMetadataParseError::New(error_type, fallback_url_,
                                             message);
    std::move(callback_).Run(nullptr, std::move(err));
    delete this;
  }

  // Implements SharedBundleDataSource::Observer.
  void OnDisconnect() override {
    RunErrorCallbackAndDestroy("Data source disconnected.");
  }

  scoped_refptr<SharedBundleDataSource> data_source_;
  ParseMetadataCallback callback_;
  bool version_mismatch_ = false;
  GURL fallback_url_;
  SectionOffsets section_offsets_;
  mojom::BundleMetadataPtr metadata_;

  base::WeakPtrFactory<MetadataParser> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MetadataParser);
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
  ~ResponseParser() override { data_source_->RemoveObserver(this); }

  void Start(uint64_t buffer_size = kInitialBufferSizeForResponse) {
    const uint64_t length = std::min(response_length_, buffer_size);
    data_source_->Read(response_offset_, length,
                       base::BindOnce(&ResponseParser::ParseResponseHeader,
                                      weak_factory_.GetWeakPtr(), length));
  }

 private:
  // https://wicg.github.io/webpackage/draft-yasskin-wpack-bundled-exchanges.html#load-response
  void ParseResponseHeader(uint64_t expected_data_length,
                           const base::Optional<std::vector<uint8_t>>& data) {
    // Step 1. "Seek to offset requestMetadata.offset in stream. If this fails,
    // return an error."
    if (!data || data->size() != expected_data_length) {
      RunErrorCallbackAndDestroy("Error reading response header.");
      return;
    }
    InputReader input(*data);

    // Step 2. "Read 1 byte from stream. If this is an error or isn't 0x82,
    // return an error."
    auto num_elements = input.ReadCBORHeader(CBORType::kArray);
    if (!num_elements || *num_elements != 2) {
      RunErrorCallbackAndDestroy("Array size of response must be 2.");
      return;
    }

    // Step 3. "Let headerLength be the result of getting the length of a CBOR
    // bytestring header from stream (Section 3.5.2). If headerLength is an
    // error, return that error."
    auto header_length = input.ReadCBORHeader(CBORType::kByteString);
    if (!header_length) {
      RunErrorCallbackAndDestroy("Cannot parse response header length.");
      return;
    }

    // Step 4. "If headerLength is 524288 (512*1024) or greater, return an
    // error."
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

    // Step 5. "Let headerCbor be the result of reading headerLength bytes from
    // stream and parsing a CBOR item from them matching the headers CDDL rule.
    // If either the read or parse returns an error, return that error."
    auto headers_bytes = input.ReadBytes(*header_length);
    if (!headers_bytes) {
      RunErrorCallbackAndDestroy("Cannot read response headers.");
      return;
    }
    cbor::Reader::DecoderError error;
    base::Optional<cbor::Value> headers_value =
        cbor::Reader::Read(*headers_bytes, &error);
    if (!headers_value) {
      RunErrorCallbackAndDestroy("Cannot parse response headers.");
      return;
    }

    // Step 6. "Let (headers, pseudos) be the result of converting headerCbor
    // to a header list and pseudoheaders using the algorithm in Section 3.6.
    // If this returns an error, return that error."
    auto parsed_headers = ConvertCBORValueToHeaders(*headers_value);
    if (!parsed_headers) {
      RunErrorCallbackAndDestroy("Cannot parse response headers.");
      return;
    }

    // Step 7. "If pseudos does not have a key named ':status' or its size
    // isn't 1, return an error."
    const auto pseudo_status = parsed_headers->pseudos.find(":status");
    if (parsed_headers->pseudos.size() != 1 ||
        pseudo_status == parsed_headers->pseudos.end()) {
      RunErrorCallbackAndDestroy(
          "Response headers map must have exactly one pseudo-header, :status.");
      return;
    }

    // Step 8. "If pseudos[':status'] isn't exactly 3 ASCII decimal digits,
    // return an error."
    int status;
    const auto& status_str = pseudo_status->second;
    if (status_str.size() != 3 ||
        !std::all_of(status_str.begin(), status_str.end(),
                     base::IsAsciiDigit<char>) ||
        !base::StringToInt(status_str, &status)) {
      RunErrorCallbackAndDestroy(":status must be 3 ASCII decimal digits.");
      return;
    }

    // Step 9. "Let payloadLength be the result of getting the length of a CBOR
    // bytestring header from stream (Section 3.5.2). If payloadLength is an
    // error, return that error."
    auto payload_length = input.ReadCBORHeader(CBORType::kByteString);
    if (!payload_length) {
      RunErrorCallbackAndDestroy("Cannot parse response payload length.");
      return;
    }

    // Step 10. "If payloadLength is greater than 0 and headers does not contain
    // a Content-Type header, return an error."
    if (*payload_length > 0 &&
        !parsed_headers->headers.contains("content-type")) {
      RunErrorCallbackAndDestroy(
          "Non-empty response must have a content-type header.");
      return;
    }

    // Step 11. "If stream.currentOffset + payloadLength !=
    // requestMetadata.offset + requestMetadata.length, return an error."
    if (input.CurrentOffset() + *payload_length != response_length_) {
      RunErrorCallbackAndDestroy("Unexpected payload length.");
      return;
    }

    // Step 12. "Let body be a new body ([FETCH]) whose stream is a tee'd copy
    // of stream starting at the current offset and ending after payloadLength
    // bytes."

    // Step 13. "Let response be a new response ([FETCH]) whose:
    // - Url list is request's url list,
    // - status is pseudos[':status'],
    // - header list is headers, and
    // - body is body."
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
    RunErrorCallbackAndDestroy("Data source disconnected.");
  }

  scoped_refptr<SharedBundleDataSource> data_source_;
  uint64_t response_offset_;
  uint64_t response_length_;
  ParseResponseCallback callback_;

  base::WeakPtrFactory<ResponseParser> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ResponseParser);
};

WebBundleParser::SharedBundleDataSource::SharedBundleDataSource(
    mojo::PendingRemote<mojom::BundleDataSource> pending_data_source)
    : data_source_(std::move(pending_data_source)) {
  data_source_.set_disconnect_handler(base::BindOnce(
      &SharedBundleDataSource::OnDisconnect, base::Unretained(this)));
}

void WebBundleParser::SharedBundleDataSource::AddObserver(Observer* observer) {
  DCHECK(observers_.end() == observers_.find(observer));
  observers_.insert(observer);
}

void WebBundleParser::SharedBundleDataSource::RemoveObserver(
    Observer* observer) {
  auto it = observers_.find(observer);
  DCHECK(observers_.end() != it);
  observers_.erase(it);
}

WebBundleParser::SharedBundleDataSource::~SharedBundleDataSource() = default;

void WebBundleParser::SharedBundleDataSource::OnDisconnect() {
  for (auto* observer : observers_)
    observer->OnDisconnect();
}

void WebBundleParser::SharedBundleDataSource::Read(
    uint64_t offset,
    uint64_t length,
    mojom::BundleDataSource::ReadCallback callback) {
  data_source_->Read(offset, length, std::move(callback));
}

WebBundleParser::WebBundleParser(
    mojo::PendingReceiver<mojom::WebBundleParser> receiver,
    mojo::PendingRemote<mojom::BundleDataSource> data_source)
    : receiver_(this, std::move(receiver)),
      data_source_(base::MakeRefCounted<SharedBundleDataSource>(
          std::move(data_source))) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &base::DeletePointer<WebBundleParser>, base::Unretained(this)));
}

WebBundleParser::~WebBundleParser() = default;

void WebBundleParser::ParseMetadata(ParseMetadataCallback callback) {
  MetadataParser* parser =
      new MetadataParser(data_source_, std::move(callback));
  parser->Start();
}

void WebBundleParser::ParseResponse(uint64_t response_offset,
                                    uint64_t response_length,
                                    ParseResponseCallback callback) {
  ResponseParser* parser = new ResponseParser(
      data_source_, response_offset, response_length, std::move(callback));
  parser->Start();
}

}  // namespace web_package
