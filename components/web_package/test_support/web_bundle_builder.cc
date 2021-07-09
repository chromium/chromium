// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/test_support/web_bundle_builder.h"

namespace web_package {
namespace test {

namespace {

cbor::Value CreateByteString(base::StringPiece s) {
  return cbor::Value(base::as_bytes(base::make_span(s)));
}

cbor::Value CreateHeaderMap(const WebBundleBuilder::Headers& headers) {
  cbor::Value::MapValue map;
  for (const auto& pair : headers)
    map.insert({CreateByteString(pair.first), CreateByteString(pair.second)});
  return cbor::Value(std::move(map));
}

}  // namespace

WebBundleBuilder::WebBundleBuilder(const std::string& fallback_url,
                                   const std::string& manifest_url)
    : fallback_url_(fallback_url) {
  writer_config_.allow_invalid_utf8_for_testing = true;
  if (!manifest_url.empty()) {
    AddSection("manifest",
               cbor::Value::InvalidUTF8StringValueForTesting(manifest_url));
  }
}

WebBundleBuilder::~WebBundleBuilder() = default;

void WebBundleBuilder::AddExchange(base::StringPiece url,
                                   const Headers& response_headers,
                                   base::StringPiece payload) {
  AddIndexEntry(url, "", {AddResponse(response_headers, payload)});
}

WebBundleBuilder::ResponseLocation WebBundleBuilder::AddResponse(
    const Headers& headers,
    base::StringPiece payload) {
  // We assume that the size of the CBOR header of the responses array is 1,
  // which is true only if the responses array has no more than 23 elements.
  DCHECK_LT(responses_.size(), 23u)
      << "WebBundleBuilder cannot create bundles with more than 23 responses";

  cbor::Value::ArrayValue response_array;
  response_array.emplace_back(Encode(CreateHeaderMap(headers)));
  response_array.emplace_back(CreateByteString(payload));
  cbor::Value response(response_array);
  int64_t response_length = EncodedLength(response);
  ResponseLocation result = {current_responses_offset_, response_length};
  current_responses_offset_ += response_length;
  responses_.emplace_back(std::move(response));
  return result;
}

void WebBundleBuilder::AddIndexEntry(
    base::StringPiece url,
    base::StringPiece variants_value,
    std::vector<ResponseLocation> response_locations) {
  cbor::Value::ArrayValue index_value_array;
  index_value_array.emplace_back(CreateByteString(variants_value));
  for (const auto& location : response_locations) {
    index_value_array.emplace_back(location.offset);
    index_value_array.emplace_back(location.length);
  }
  index_.insert({cbor::Value::InvalidUTF8StringValueForTesting(url),
                 cbor::Value(index_value_array)});
}

void WebBundleBuilder::AddSection(base::StringPiece name, cbor::Value section) {
  section_lengths_.emplace_back(name);
  section_lengths_.emplace_back(EncodedLength(section));
  sections_.emplace_back(std::move(section));
}

void WebBundleBuilder::AddAuthority(cbor::Value::MapValue authority) {
  authorities_.emplace_back(std::move(authority));
}

void WebBundleBuilder::AddVouchedSubset(cbor::Value::MapValue vouched_subset) {
  vouched_subsets_.emplace_back(std::move(vouched_subset));
}

std::vector<uint8_t> WebBundleBuilder::CreateBundle() {
  AddSection("index", cbor::Value(index_));
  if (!authorities_.empty() || !vouched_subsets_.empty()) {
    cbor::Value::ArrayValue signatures_section;
    signatures_section.emplace_back(std::move(authorities_));
    signatures_section.emplace_back(std::move(vouched_subsets_));
    AddSection("signatures", cbor::Value(std::move(signatures_section)));
  }
  AddSection("responses", cbor::Value(responses_));
  return Encode(CreateTopLevel());
}

cbor::Value WebBundleBuilder::CreateEncodedSigned(
    base::StringPiece validity_url,
    base::StringPiece auth_sha256,
    int64_t date,
    int64_t expires,
    base::StringPiece url,
    base::StringPiece header_sha256,
    base::StringPiece payload_integrity_header) {
  cbor::Value::ArrayValue subset_hash_value;
  subset_hash_value.emplace_back(CreateByteString(""));  // variants-value
  subset_hash_value.emplace_back(CreateByteString(header_sha256));
  subset_hash_value.emplace_back(payload_integrity_header);

  cbor::Value::MapValue subset_hashes;
  subset_hashes.emplace(url, std::move(subset_hash_value));

  cbor::Value::MapValue signed_subset;
  signed_subset.emplace("validity-url", validity_url);
  signed_subset.emplace("auth-sha256", CreateByteString(auth_sha256));
  signed_subset.emplace("date", date);
  signed_subset.emplace("expires", expires);
  signed_subset.emplace("subset-hashes", std::move(subset_hashes));
  return cbor::Value(Encode(cbor::Value(signed_subset)));
}

cbor::Value WebBundleBuilder::CreateTopLevel() {
  cbor::Value::ArrayValue toplevel_array;
  toplevel_array.emplace_back(
      CreateByteString(u8"\U0001F310\U0001F4E6"));  // "🌐📦"
  toplevel_array.emplace_back(CreateByteString(base::StringPiece("b1\0\0", 4)));
  toplevel_array.emplace_back(
      cbor::Value::InvalidUTF8StringValueForTesting(fallback_url_));
  toplevel_array.emplace_back(Encode(cbor::Value(section_lengths_)));
  toplevel_array.emplace_back(sections_);
  toplevel_array.emplace_back(CreateByteString(""));  // length (ignored)
  return cbor::Value(toplevel_array);
}

std::vector<uint8_t> WebBundleBuilder::Encode(const cbor::Value& value) {
  return *cbor::Writer::Write(value, writer_config_);
}

int64_t WebBundleBuilder::EncodedLength(const cbor::Value& value) {
  return Encode(value).size();
}

}  // namespace test
}  // namespace web_package
