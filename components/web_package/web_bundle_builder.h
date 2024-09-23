// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_BUILDER_H_
#define COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_BUILDER_H_

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "components/cbor/writer.h"
#include "url/gurl.h"

namespace web_package {

enum class BundleVersion {
  kB2,
};

// This class can be used to create a Web Bundle.
class WebBundleBuilder {
 public:
  using Headers = std::vector<std::pair<std::string, std::string>>;
  struct ResponseLocation {
    // /components/cbor uses int64_t for integer types.
    int64_t offset;
    int64_t length;
  };

  explicit WebBundleBuilder(
      BundleVersion version = BundleVersion::kB2,
      bool allow_invalid_utf8_strings_for_testing = false);

  ~WebBundleBuilder();

  // Add an exchange to the Web Bundle for a given `GURL`.
  void AddExchange(const GURL& url,
                   const Headers& response_headers,
                   std::string_view payload);
  // Add an exchange to the Web Bundle for a given `url` represented as a
  // string. In contrast to providing the URL as `GURL`, this allows adding
  // relative URLs to the Web Bundle.
  void AddExchange(std::string_view url,
                   const Headers& response_headers,
                   std::string_view payload);

  ResponseLocation AddResponse(const Headers& headers,
                               std::string_view payload);

  // Adds an entry to the "index" section of the Web Bundle for the given
  // `GURL`.
  void AddIndexEntry(const GURL& url,
                     const ResponseLocation& response_location);
  // Adds an entry to the "index" section of the Web Bundle  for the given `url`
  // represented as a string. In contrast to providing the URL as `GURL`, this
  // allows adding relative URLs to the Web Bundle.
  void AddIndexEntry(std::string_view url,
                     const ResponseLocation& response_location);

  void AddSection(std::string_view name, cbor::Value section);

  // Adds a "primary" section to the Web Bundle containing a given `GURL`.
  void AddPrimaryURL(const GURL& url);
  // Adds a "primary" section to the Web Bundle for a given `url` represented as
  // a string. In contrast to providing the URL as `GURL`, this allows setting
  // relative URLs as the primary URL of a Web Bundle.
  void AddPrimaryURL(std::string_view url);

  std::vector<uint8_t> CreateBundle();

 private:
  std::vector<uint8_t> CreateTopLevel();
  std::vector<uint8_t> Encode(const cbor::Value& value);
  cbor::Value GetCborValueOfURL(std::string_view url);

  int64_t EncodedLength(const cbor::Value& value);

  cbor::Writer::Config writer_config_;
  cbor::Value::ArrayValue section_lengths_;
  cbor::Value::ArrayValue sections_;
  std::map<std::string, ResponseLocation> delayed_index_;
  cbor::Value::ArrayValue responses_;
  BundleVersion version_;
  int64_t current_responses_offset_ = 0;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_BUILDER_H_
