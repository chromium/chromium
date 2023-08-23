// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_builder.h"

#include <string.h>

#include <ostream>

#include "base/big_endian.h"

namespace web_package {

namespace {

cbor::Value CreateByteString(base::StringPiece s) {
  return cbor::Value(base::as_bytes(base::make_span(s)));
}

cbor::Value CreateHeaderMap(const WebBundleBuilder::Headers& headers) {
  cbor::Value::MapValue map;
  for (const auto& pair : headers) {
    map.insert({CreateByteString(pair.first), CreateByteString(pair.second)});
  }
  return cbor::Value(std::move(map));
}

// TODO(myrzakereyms): replace this method with cbor::writer::GetNumUintBytes.
uint64_t GetNumUintBytes(uint64_t value) {
  if (value < 24) {
    return 0;
  } else if (value <= 0xFF) {
    return 1;
  } else if (value <= 0xFFFF) {
    return 2;
  } else if (value <= 0xFFFFFFFF) {
    return 4;
  }
  return 8;
}

}  // namespace

WebBundleBuilder::WebBundleBuilder(BundleVersion version,
                                   bool allow_invalid_utf8_strings_for_testing)
    : version_(version) {
  // Currently the only supported bundle format is b2.
  DCHECK_EQ(version_, BundleVersion::kB2);
  writer_config_.allow_invalid_utf8_for_testing =
      allow_invalid_utf8_strings_for_testing;
}
WebBundleBuilder::~WebBundleBuilder() = default;

cbor::Value WebBundleBuilder::GetCborValueOfURL(base::StringPiece url) {
  if (writer_config_.allow_invalid_utf8_for_testing) {
    return cbor::Value::InvalidUTF8StringValueForTesting(url);
  }
  return cbor::Value(url);
}

void WebBundleBuilder::AddExchange(const GURL& url,
                                   const Headers& response_headers,
                                   base::StringPiece payload) {
  AddExchange(url.spec(), response_headers, payload);
}

void WebBundleBuilder::AddExchange(base::StringPiece url,
                                   const Headers& response_headers,
                                   base::StringPiece payload) {
  AddIndexEntry(url, AddResponse(response_headers, payload));
}

WebBundleBuilder::ResponseLocation WebBundleBuilder::AddResponse(
    const Headers& headers,
    base::StringPiece payload) {
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
    const GURL& url,
    const ResponseLocation& response_location) {
  AddIndexEntry(url.spec(), response_location);
}

void WebBundleBuilder::AddIndexEntry(
    base::StringPiece url,
    const ResponseLocation& response_location) {
  delayed_index_.insert({std::string{url}, response_location});
}

void WebBundleBuilder::AddSection(base::StringPiece name, cbor::Value section) {
  section_lengths_.emplace_back(name);
  section_lengths_.emplace_back(EncodedLength(section));
  sections_.emplace_back(std::move(section));
}

void WebBundleBuilder::AddPrimaryURL(const GURL& url) {
  AddPrimaryURL(url.spec());
}

void WebBundleBuilder::AddPrimaryURL(base::StringPiece url) {
  AddSection("primary", GetCborValueOfURL(url));
}

std::vector<uint8_t> WebBundleBuilder::CreateBundle() {
  // Now that we know how many responses will be in the bundle,
  // we want to shift all the offsets by the bytes required
  // for the CBOR Array header and actually construct the index
  // section.
  int64_t initial_offset = 1 + GetNumUintBytes(responses_.size());
  cbor::Value::MapValue index;
  for (auto& entry : delayed_index_) {
    const ResponseLocation& location = entry.second;
    cbor::Value::ArrayValue index_value_array;
    index_value_array.emplace_back(location.offset + initial_offset);
    index_value_array.emplace_back(location.length);
    index.insert(
        {GetCborValueOfURL(entry.first), cbor::Value(index_value_array)});
  }
  AddSection("index", cbor::Value(index));
  AddSection("responses", cbor::Value(responses_));
  return CreateTopLevel();
}

std::vector<uint8_t> WebBundleBuilder::CreateTopLevel() {
  cbor::Value::ArrayValue toplevel_array;
  toplevel_array.emplace_back(CreateByteString("🌐📦"));
  toplevel_array.emplace_back(CreateByteString(base::StringPiece("b2\0\0", 4)));
  toplevel_array.emplace_back(Encode(cbor::Value(section_lengths_)));
  toplevel_array.emplace_back(sections_);
  // Put a dummy 8-byte bytestring.
  toplevel_array.emplace_back(cbor::Value::BinaryValue(8, 0));

  std::vector<uint8_t> bundle = Encode(cbor::Value(toplevel_array));
  char encoded[8];
  base::WriteBigEndian(encoded, static_cast<uint64_t>(bundle.size()));
  // Overwrite the dummy bytestring with the actual size.
  memcpy(bundle.data() + bundle.size() - 8, encoded, 8);
  return bundle;
}

std::vector<uint8_t> WebBundleBuilder::Encode(const cbor::Value& value) {
  return *cbor::Writer::Write(value, writer_config_);
}

int64_t WebBundleBuilder::EncodedLength(const cbor::Value& value) {
  return Encode(value).size();
}

}  // namespace web_package
