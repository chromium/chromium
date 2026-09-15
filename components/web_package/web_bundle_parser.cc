// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_parser.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/rust/web_package_rust.h"
#include "components/web_package/signed_web_bundles/integrity_block_parser.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace web_package {

namespace {


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
    const uint64_t unsigned_file_length =
        base::checked_cast<uint64_t>(file_length);
    if (unsigned_file_length < rust::TRAILING_LENGTH_NUM_BYTES) {
      RunErrorCallback("Error reading bundle length.");
      return;
    }

    // Read the last 8 bytes of the file that correspond to the trailing length
    // field of the web bundle.
    data_source_->get()->Read(
        unsigned_file_length - rust::TRAILING_LENGTH_NUM_BYTES,
        rust::TRAILING_LENGTH_NUM_BYTES,
        base::BindOnce(&MetadataParser::ParseWebBundleLength,
                       weak_factory_.GetWeakPtr(), unsigned_file_length));
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
    // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-trailing-length
    auto bundle_offset = rust::parse_trailing_length(*data, file_length);
    if (!bundle_offset.has_value()) {
      RunErrorCallback(bundle_offset.error().message);
      return;
    }
    ReadMagicBytes(*bundle_offset);
  }

  void ReadMagicBytes(const uint64_t offset_in_stream) {
    // First, we will parse the CBOR header of the top level array (1-byte),
    // `magic`, `version`, and the CBOR header of `section-lengths`.
    data_source_->get()->Read(
        offset_in_stream, rust::INITIAL_BUNDLE_HEADER_BUFFER_SIZE,
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

    auto res = rust::parse_magic_and_version(*data, offset_in_stream);
    if (!res.has_value()) {
      const auto& error = res.error();
      RunErrorCallback(error.message,
                       error.is_version_error
                           ? mojom::BundleParseErrorType::kVersionError
                           : mojom::BundleParseErrorType::kFormatError);
      return;
    }

    uint64_t next_offset = res->next_read_offset;
    uint64_t next_length = res->next_read_length;
    data_source_->get()->Read(
        next_offset, next_length,
        base::BindOnce(&MetadataParser::ParseBundleHeader,
                       weak_factory_.GetWeakPtr(), next_offset,
                       res->section_lengths_len));
  }

  void ParseBundleHeader(uint64_t offset_in_stream,
                         uint64_t section_lengths_length,
                         const std::optional<std::vector<uint8_t>>& data) {
    if (!data) {
      RunErrorCallback("Error reading bundle header.");
      return;
    }

    auto res = rust::parse_bundle_header(*data, section_lengths_length,
                                         offset_in_stream);
    if (!res.has_value()) {
      RunErrorCallback(res.error().message);
      return;
    }

    responses_section_offset_ = res->responses_offset;
    responses_section_length_ = res->responses_length;
    metadata_sections_to_read_ = std::move(res->metadata_sections);

    metadata_ = mojom::BundleMetadata::New();
    metadata_->version = mojom::BundleFormatVersion::kB2;

    ReadMetadataSections(/*section_index=*/0);
  }

  // https://www.ietf.org/archive/id/draft-ietf-wpack-bundled-responses-01.html#name-bundle-sections
  void ReadMetadataSections(size_t section_index) {
    if (section_index < metadata_sections_to_read_.size()) {
      const auto& section = metadata_sections_to_read_[section_index];
      if (section.length > rust::MAX_METADATA_SECTION_SIZE) {
        RunErrorCallback(
            "Metadata sections larger than 1MB are not supported.");
        return;
      }

      data_source_->get()->Read(
          section.offset, section.length,
          base::BindOnce(&MetadataParser::ParseMetadataSection,
                         weak_factory_.GetWeakPtr(), section_index,
                         section.length));
      return;
    }

    // The bundle MUST contain the "index" and "responses" sections.
    if (metadata_->requests.empty()) {
      RunErrorCallback("Bundle must have an index section.");
      return;
    }

    RunSuccessCallback();
  }

  void ParseMetadataSection(size_t section_index,
                            uint64_t expected_data_length,
                            const std::optional<std::vector<uint8_t>>& data) {
    if (!data || data->size() != expected_data_length) {
      RunErrorCallback("Error reading section content.");
      return;
    }

    const auto& name = metadata_sections_to_read_[section_index].name;
    if (name == rust::INDEX_SECTION) {
      auto res = rust::parse_index_section(*data, responses_section_offset_,
                                           responses_section_length_);
      if (!res.has_value()) {
        RunErrorCallback(res.error().message);
        return;
      }
      std::vector<std::pair<GURL, mojom::BundleResponseLocationPtr>> requests;
      requests.reserve(res->size());
      for (const auto& entry : *res) {
        GURL parsed_url = ParseExchangeURL(entry.url, base_url_);
        if (!parsed_url.is_valid()) {
          std::string message = base::StrCat({"Index section: exchange URL \"",
                                              entry.url, "\" is not valid."});
          if (base_url_.is_empty()) {
            message += " (Relative URLs are not allowed in this context.)";
          }
          RunErrorCallback(message);
          return;
        }
        requests.emplace_back(
            std::move(parsed_url),
            mojom::BundleResponseLocation::New(entry.offset, entry.length));
      }
      metadata_->requests =
          base::flat_map<GURL, mojom::BundleResponseLocationPtr>(
              std::move(requests));
    } else if (name == rust::CRITICAL_SECTION) {
      if (auto res = rust::parse_critical_section(*data); !res.has_value()) {
        RunErrorCallback(res.error().message);
        return;
      }
    } else if (name == rust::PRIMARY_SECTION) {
      auto res = rust::parse_primary_section(*data);
      if (!res.has_value()) {
        RunErrorCallback(res.error().message);
        return;
      }
      GURL parsed_url = ParseExchangeURL(*res, base_url_);
      if (!parsed_url.is_valid()) {
        RunErrorCallback("Primary URL is not a valid exchange URL.");
        return;
      }
      metadata_->primary_url = std::move(parsed_url);
    } else {
      NOTREACHED();
    }
    // Read the next metadata section.
    ReadMetadataSections(section_index + 1);
  }

  void RunSuccessCallback() {
    std::move(complete_callback_)
        .Run(base::BindOnce(std::move(result_callback_), std::move(metadata_),
                            nullptr));
  }

  void RunErrorCallback(std::string_view message,
                        mojom::BundleParseErrorType error_type =
                            mojom::BundleParseErrorType::kFormatError) {
    DLOG(ERROR) << "Parsing web bundle error: " << message;
    // Avoids an extra temporary std::string allocation in Mojo's New().
    auto err = mojom::BundleMetadataParseError::New();
    err->type = error_type;
    err->message = message;
    std::move(complete_callback_)
        .Run(base::BindOnce(std::move(result_callback_), nullptr,
                            std::move(err)));
  }

  const raw_ref<mojo::Remote<mojom::BundleDataSource>> data_source_;
  const GURL base_url_;
  std::optional<uint64_t> start_reading_offset_;
  ParseMetadataCallback result_callback_;
  ParsingCompleteCallback complete_callback_;
  rs_std::Vec<rust::SectionOffsetEntry> metadata_sections_to_read_;
  uint64_t responses_section_offset_ = 0;
  uint64_t responses_section_length_ = 0;
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
    StartWithBufferSize(rust::INITIAL_BUFFER_SIZE_FOR_RESPONSE);
  }

 private:
  void StartWithBufferSize(uint64_t buffer_size) {
    const uint64_t length = std::min(response_length_, buffer_size);
    data_source_->get()->Read(
        response_offset_, length,
        base::BindOnce(&ResponseParser::ParseResponseHeader,
                       weak_factory_.GetWeakPtr(), length));
  }
  void ParseResponseHeader(uint64_t expected_data_length,
                           const std::optional<std::vector<uint8_t>>& data) {
    if (!data || data->size() != expected_data_length) {
      RunErrorCallback("Error reading response header.");
      return;
    }

    auto parse_res =
        rust::parse_response(*data, response_offset_, response_length_);
    if (!parse_res.has_value()) {
      RunErrorCallback(parse_res.error().message);
      return;
    }
    if (parse_res->needs_more_data) {
      DVLOG(1) << "Re-reading response header with a buffer of size "
               << parse_res->required_buffer_size;
      StartWithBufferSize(parse_res->required_buffer_size);
      return;
    }

    mojom::BundleResponsePtr response = mojom::BundleResponse::New();
    response->response_code = parse_res->response_code;
    response->response_headers = base::MakeFlatMap<std::string, std::string>(
        parse_res->headers, {}, [](const rust::HeaderEntry& header) {
          return std::make_pair(
              std::string(base::as_string_view(header.name.to_span())),
              std::string(base::as_string_view(header.value.to_span())));
        });
    response->payload_offset = parse_res->payload_offset;
    response->payload_length = parse_res->payload_length;
    RunSuccessCallback(std::move(response));
  }

  void RunSuccessCallback(mojom::BundleResponsePtr response) {
    std::move(complete_callback_)
        .Run(base::BindOnce(std::move(result_callback_), std::move(response),
                            nullptr));
  }

  void RunErrorCallback(const std::string_view message,
                        mojom::BundleParseErrorType error_type =
                            mojom::BundleParseErrorType::kFormatError) {
    auto err = mojom::BundleResponseParseError::New();
    err->type = error_type;
    err->message = message;
    std::move(complete_callback_)
        .Run(base::BindOnce(std::move(result_callback_), nullptr,
                            std::move(err)));
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
