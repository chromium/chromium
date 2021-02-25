// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_parser.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/cbor/writer.h"
#include "components/web_package/test_support/web_bundle_builder.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

constexpr char kFallbackUrl[] = "https://test.example.com/";
constexpr char kManifestUrl[] = "https://test.example.com/manifest";
constexpr char kValidityUrl[] =
    "https://test.example.org/resource.validity.msg";
const uint64_t kSignatureDate = 1564272000;  // 2019-07-28T00:00:00Z
const uint64_t kSignatureDuration = 7 * 24 * 60 * 60;

std::string GetTestFileContents(const base::FilePath& path) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.Append(
      base::FilePath(FILE_PATH_LITERAL("components/test/data/web_package")));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(test_data_dir.Append(path), &contents));
  return contents;
}

class TestDataSource : public mojom::BundleDataSource {
 public:
  explicit TestDataSource(const base::FilePath& path)
      : data_(GetTestFileContents(path)) {}
  explicit TestDataSource(const std::vector<uint8_t>& data)
      : data_(reinterpret_cast<const char*>(data.data()), data.size()) {}

  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    if (offset >= data_.size()) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    const uint8_t* start =
        reinterpret_cast<const uint8_t*>(data_.data()) + offset;
    uint64_t available_length = std::min(length, data_.size() - offset);
    std::move(callback).Run(
        std::vector<uint8_t>(start, start + available_length));
  }

  base::StringPiece GetPayload(const mojom::BundleResponsePtr& response) {
    return base::StringPiece(data_).substr(response->payload_offset,
                                           response->payload_length);
  }

  void AddReceiver(mojo::PendingReceiver<mojom::BundleDataSource> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  std::string data_;
  mojo::ReceiverSet<mojom::BundleDataSource> receivers_;
};

using ParseBundleResult =
    std::pair<mojom::BundleMetadataPtr, mojom::BundleMetadataParseErrorPtr>;

ParseBundleResult ParseBundle(TestDataSource* data_source) {
  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  data_source->AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::WebBundleParser> parser_remote;
  WebBundleParser parser_impl(parser_remote.InitWithNewPipeAndPassReceiver(),
                              std::move(source_remote));
  mojom::WebBundleParser& parser = parser_impl;

  base::RunLoop run_loop;
  ParseBundleResult result;
  parser.ParseMetadata(base::BindLambdaForTesting(
      [&result, &run_loop](mojom::BundleMetadataPtr metadata,
                           mojom::BundleMetadataParseErrorPtr error) {
        result = std::make_pair(std::move(metadata), std::move(error));
        run_loop.QuitClosure().Run();
      }));
  run_loop.Run();
  EXPECT_TRUE((result.first && !result.second) ||
              (!result.first && result.second));
  return result;
}

void ExpectFormatErrorWithFallbackURL(ParseBundleResult result) {
  ASSERT_TRUE(result.second);
  EXPECT_EQ(result.second->type, mojom::BundleParseErrorType::kFormatError);
  EXPECT_EQ(result.second->fallback_url, kFallbackUrl);
}

// Finds the only response for |url|. The index entry for |url| must not have
// variants-value.
mojom::BundleResponseLocationPtr FindResponse(
    const mojom::BundleMetadataPtr& metadata,
    const GURL& url) {
  const auto item = metadata->requests.find(url);
  if (item == metadata->requests.end())
    return nullptr;

  const mojom::BundleIndexValuePtr& index_value = item->second;
  EXPECT_TRUE(index_value->variants_value.empty());
  EXPECT_EQ(index_value->response_locations.size(), 1u);
  if (index_value->response_locations.empty())
    return nullptr;
  return index_value->response_locations[0].Clone();
}

mojom::BundleResponsePtr ParseResponse(
    TestDataSource* data_source,
    const mojom::BundleResponseLocationPtr& location) {
  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  data_source->AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::WebBundleParser> parser_remote;
  WebBundleParser parser_impl(parser_remote.InitWithNewPipeAndPassReceiver(),
                              std::move(source_remote));
  mojom::WebBundleParser& parser = parser_impl;

  base::RunLoop run_loop;
  mojom::BundleResponsePtr result;
  parser.ParseResponse(
      location->offset, location->length,
      base::BindLambdaForTesting(
          [&result, &run_loop](mojom::BundleResponsePtr response,
                               mojom::BundleResponseParseErrorPtr error) {
            result = std::move(response);
            run_loop.QuitClosure().Run();
          }));
  run_loop.Run();
  return result;
}

cbor::Value CreateByteString(base::StringPiece s) {
  return cbor::Value(base::as_bytes(base::make_span(s)));
}

std::string AsString(const std::vector<uint8_t>& data) {
  return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

}  // namespace

class WebBundleParserTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebBundleParserTest, WrongMagic) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  std::vector<uint8_t> bundle = builder.CreateBundle();
  bundle[3] ^= 1;
  TestDataSource data_source(bundle);

  mojom::BundleMetadataParseErrorPtr error = ParseBundle(&data_source).second;
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kFormatError);
  EXPECT_TRUE(error->fallback_url.is_empty());
}

TEST_F(WebBundleParserTest, UnknownVersion) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  std::vector<uint8_t> bundle = builder.CreateBundle();
  // Modify the version string from "b1\0\0" to "q1\0\0".
  ASSERT_EQ(bundle[11], 'b');
  bundle[11] = 'q';
  TestDataSource data_source(bundle);

  mojom::BundleMetadataParseErrorPtr error = ParseBundle(&data_source).second;
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kVersionError);
  EXPECT_EQ(error->fallback_url, kFallbackUrl);
}

TEST_F(WebBundleParserTest, FallbackURLIsNotUTF8) {
  test::WebBundleBuilder builder("https://test.example.com/\xcc", kManifestUrl);
  std::vector<uint8_t> bundle = builder.CreateBundle();
  TestDataSource data_source(bundle);

  mojom::BundleMetadataParseErrorPtr error = ParseBundle(&data_source).second;
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kFormatError);
  EXPECT_TRUE(error->fallback_url.is_empty());
}

TEST_F(WebBundleParserTest, FallbackURLHasFragment) {
  test::WebBundleBuilder builder("https://test.example.com/#fragment",
                                 kManifestUrl);
  std::vector<uint8_t> bundle = builder.CreateBundle();
  TestDataSource data_source(bundle);

  mojom::BundleMetadataParseErrorPtr error = ParseBundle(&data_source).second;
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kFormatError);
  EXPECT_TRUE(error->fallback_url.is_empty());
}

TEST_F(WebBundleParserTest, SectionLengthsTooLarge) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  std::string too_long_section_name(8192, 'x');
  builder.AddSection(too_long_section_name, cbor::Value(0));
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, DuplicateSectionName) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddSection("foo", cbor::Value(0));
  builder.AddSection("foo", cbor::Value(0));
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, SingleEntry) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 1u);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
}

TEST_F(WebBundleParserTest, InvalidRequestURL) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("", {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLIsNotUTF8) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/\xcc",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLHasBadScheme) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("file:///tmp/foo",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLHasCredentials) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://user:passwd@test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLHasFragment) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/#fragment",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLIsValidUrnUuid) {
  const char urn_uuid[] = "urn:uuid:f81d4fae-7dec-11d0-a765-00a0c91e6bf6";
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange(urn_uuid,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 1u);
  auto location = FindResponse(metadata, GURL(urn_uuid));
  ASSERT_TRUE(location);
}

TEST_F(WebBundleParserTest, RequestURLIsInvalidUrnUuid) {
  const char urn_uuid[] = "urn:uuid:invalid";
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange(urn_uuid,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, NoStatusInResponseHeaders) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{"content-type", "text/plain"}},
                      "payload");  // ":status" is missing.
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, InvalidResponseStatus) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "0200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, ExtraPseudoInResponseHeaders) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange(
      "https://test.example.com/",
      {{":status", "200"}, {":foo", ""}, {"content-type", "text/plain"}},
      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, UpperCaseCharacterInHeaderName) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"Content-Type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, InvalidHeaderValue) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "\n"}}, "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, NoContentTypeWithNonEmptyContent) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/", {{":status", "200"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, NoContentTypeWithEmptyContent) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/", {{":status", "301"}}, "");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_TRUE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, Variants) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  auto location1 = builder.AddResponse(
      {{":status", "200"}, {"content-type", "text/html"}}, "payload1");
  auto location2 = builder.AddResponse(
      {{":status", "200"}, {"content-type", "text/plain"}}, "payload2");
  builder.AddIndexEntry("https://test.example.com/",
                        "Accept;text/html;text/plain", {location1, location2});
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  const auto& found =
      metadata->requests.find(GURL("https://test.example.com/"));
  ASSERT_NE(found, metadata->requests.end());
  const mojom::BundleIndexValuePtr& index_entry = found->second;
  EXPECT_EQ(index_entry->variants_value, "Accept;text/html;text/plain");
  ASSERT_EQ(index_entry->response_locations.size(), 2u);

  auto response1 =
      ParseResponse(&data_source, index_entry->response_locations[0]);
  ASSERT_TRUE(response1);
  EXPECT_EQ(data_source.GetPayload(response1), "payload1");
  auto response2 =
      ParseResponse(&data_source, index_entry->response_locations[1]);
  ASSERT_TRUE(response2);
  EXPECT_EQ(data_source.GetPayload(response2), "payload2");
}

TEST_F(WebBundleParserTest, EmptyIndexEntry) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddIndexEntry("https://test.example.com/", "", {});
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, EmptyIndexEntryWithVariants) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddIndexEntry("https://test.example.com/",
                        "Accept;text/html;text/plain", {});
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, MultipleResponsesWithoutVariantsValue) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  auto location1 = builder.AddResponse(
      {{":status", "200"}, {"content-type", "text/html"}}, "payload1");
  auto location2 = builder.AddResponse(
      {{":status", "200"}, {"content-type", "text/plain"}}, "payload2");
  builder.AddIndexEntry("https://test.example.com/", "",
                        {location1, location2});
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, AllKnownSectionInCritical) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  cbor::Value::ArrayValue critical_section;
  critical_section.emplace_back("manifest");
  critical_section.emplace_back("index");
  critical_section.emplace_back("critical");
  critical_section.emplace_back("responses");
  builder.AddSection("critical", cbor::Value(critical_section));
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
}

TEST_F(WebBundleParserTest, UnknownSectionInCritical) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  cbor::Value::ArrayValue critical_section;
  critical_section.emplace_back("unknown_section_name");
  builder.AddSection("critical", cbor::Value(critical_section));
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, NoManifest) {
  test::WebBundleBuilder builder(kFallbackUrl, std::string());
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
}

TEST_F(WebBundleParserTest, InvalidManifestURL) {
  test::WebBundleBuilder builder(kFallbackUrl, "not-an-absolute-url");
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatErrorWithFallbackURL(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, EmptySignaturesSection) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  // test::WebBundleBuilder omits signatures section if empty, so create it
  // ourselves.
  cbor::Value::ArrayValue signatures_section;
  signatures_section.emplace_back(cbor::Value::ArrayValue());  // authorities
  signatures_section.emplace_back(
      cbor::Value::ArrayValue());  // vouched-subsets
  builder.AddSection("signatures", cbor::Value(signatures_section));
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  EXPECT_TRUE(metadata->authorities.empty());
  EXPECT_TRUE(metadata->vouched_subsets.empty());
}

TEST_F(WebBundleParserTest, SignaturesSection) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");

  // Create a signatures section with some dummy data.
  cbor::Value::MapValue authority;
  authority.emplace("cert", CreateByteString("[cert]"));
  authority.emplace("ocsp", CreateByteString("[ocsp]"));
  authority.emplace("sct", CreateByteString("[sct]"));
  builder.AddAuthority(std::move(authority));

  cbor::Value signed_bytes = builder.CreateEncodedSigned(
      kValidityUrl, "[auth-sha256]", kSignatureDate,
      kSignatureDate + kSignatureDuration, "https://test.example.com/",
      "[header-sha256]", "[payload-integrity]");
  cbor::Value::MapValue vouched_subset;
  vouched_subset.emplace("authority", 0);
  vouched_subset.emplace("sig", CreateByteString("[sig]"));
  vouched_subset.emplace("signed", signed_bytes.Clone());
  builder.AddVouchedSubset(std::move(vouched_subset));

  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);

  ASSERT_EQ(metadata->authorities.size(), 1u);
  EXPECT_EQ(AsString(metadata->authorities[0]->cert), "[cert]");
  ASSERT_TRUE(metadata->authorities[0]->ocsp.has_value());
  EXPECT_EQ(AsString(*metadata->authorities[0]->ocsp), "[ocsp]");
  ASSERT_TRUE(metadata->authorities[0]->sct.has_value());
  EXPECT_EQ(AsString(*metadata->authorities[0]->sct), "[sct]");

  ASSERT_EQ(metadata->vouched_subsets.size(), 1u);
  EXPECT_EQ(metadata->vouched_subsets[0]->authority, 0u);
  EXPECT_EQ(AsString(metadata->vouched_subsets[0]->sig), "[sig]");
  EXPECT_EQ(AsString(metadata->vouched_subsets[0]->raw_signed),
            signed_bytes.GetBytestringAsString());

  const auto& parsed_signed = metadata->vouched_subsets[0]->parsed_signed;
  EXPECT_EQ(parsed_signed->validity_url, kValidityUrl);
  EXPECT_EQ(AsString(parsed_signed->auth_sha256), "[auth-sha256]");
  EXPECT_EQ(parsed_signed->date, kSignatureDate);
  EXPECT_EQ(parsed_signed->expires, kSignatureDate + kSignatureDuration);

  EXPECT_EQ(parsed_signed->subset_hashes.size(), 1u);
  const auto& hashes =
      parsed_signed->subset_hashes[GURL("https://test.example.com/")];
  ASSERT_TRUE(hashes);
  EXPECT_EQ(hashes->variants_value, "");
  ASSERT_EQ(hashes->resource_integrities.size(), 1u);
  EXPECT_EQ(AsString(hashes->resource_integrities[0]->header_sha256),
            "[header-sha256]");
  EXPECT_EQ(hashes->resource_integrities[0]->payload_integrity_header,
            "[payload-integrity]");
}

TEST_F(WebBundleParserTest, MultipleSignatures) {
  test::WebBundleBuilder builder(kFallbackUrl, kManifestUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");

  // Create a signatures section with some dummy data.
  cbor::Value::MapValue authority1;
  authority1.emplace("cert", CreateByteString("[cert1]"));
  authority1.emplace("ocsp", CreateByteString("[ocsp]"));
  authority1.emplace("sct", CreateByteString("[sct]"));
  builder.AddAuthority(std::move(authority1));
  cbor::Value::MapValue authority2;
  authority2.emplace("cert", CreateByteString("[cert2]"));
  builder.AddAuthority(std::move(authority2));

  cbor::Value signed_bytes1 = builder.CreateEncodedSigned(
      kValidityUrl, "[auth-sha256]", kSignatureDate,
      kSignatureDate + kSignatureDuration, "https://test.example.com/",
      "[header-sha256]", "[payload-integrity]");
  cbor::Value::MapValue vouched_subset1;
  vouched_subset1.emplace("authority", 0);
  vouched_subset1.emplace("sig", CreateByteString("[sig1]"));
  vouched_subset1.emplace("signed", signed_bytes1.Clone());
  builder.AddVouchedSubset(std::move(vouched_subset1));

  cbor::Value signed_bytes2 = builder.CreateEncodedSigned(
      kValidityUrl, "[auth-sha256-2]", kSignatureDate,
      kSignatureDate + kSignatureDuration, "https://test.example.org/",
      "[header-sha256-2]", "[payload-integrity-2]");
  cbor::Value::MapValue vouched_subset2;
  vouched_subset2.emplace("authority", 1);
  vouched_subset2.emplace("sig", CreateByteString("[sig2]"));
  vouched_subset2.emplace("signed", signed_bytes2.Clone());
  builder.AddVouchedSubset(std::move(vouched_subset2));

  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);

  ASSERT_EQ(metadata->authorities.size(), 2u);
  EXPECT_EQ(AsString(metadata->authorities[0]->cert), "[cert1]");
  EXPECT_TRUE(metadata->authorities[0]->ocsp.has_value());
  EXPECT_TRUE(metadata->authorities[0]->sct.has_value());
  EXPECT_EQ(AsString(metadata->authorities[1]->cert), "[cert2]");
  EXPECT_FALSE(metadata->authorities[1]->ocsp.has_value());
  EXPECT_FALSE(metadata->authorities[1]->sct.has_value());

  ASSERT_EQ(metadata->vouched_subsets.size(), 2u);
  EXPECT_EQ(metadata->vouched_subsets[0]->authority, 0u);
  EXPECT_EQ(AsString(metadata->vouched_subsets[0]->sig), "[sig1]");
  EXPECT_EQ(AsString(metadata->vouched_subsets[0]->raw_signed),
            signed_bytes1.GetBytestringAsString());
  EXPECT_EQ(metadata->vouched_subsets[1]->authority, 1u);
  EXPECT_EQ(AsString(metadata->vouched_subsets[1]->sig), "[sig2]");
  EXPECT_EQ(AsString(metadata->vouched_subsets[1]->raw_signed),
            signed_bytes2.GetBytestringAsString());
}

TEST_F(WebBundleParserTest, ParseGoldenFile) {
  TestDataSource data_source(base::FilePath(FILE_PATH_LITERAL("hello.wbn")));

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 4u);
  EXPECT_EQ(metadata->manifest_url,
            "https://test.example.org/manifest.webmanifest");

  std::map<std::string, mojom::BundleResponsePtr> responses;
  for (const auto& item : metadata->requests) {
    auto location = FindResponse(metadata, item.first);
    ASSERT_TRUE(location);
    auto resp = ParseResponse(&data_source, location);
    ASSERT_TRUE(resp);
    responses[item.first.spec()] = std::move(resp);
  }

  ASSERT_TRUE(responses["https://test.example.org/"]);
  EXPECT_EQ(responses["https://test.example.org/"]->response_code, 200);
  EXPECT_EQ(
      responses["https://test.example.org/"]->response_headers["content-type"],
      "text/html; charset=utf-8");
  EXPECT_EQ(data_source.GetPayload(responses["https://test.example.org/"]),
            GetTestFileContents(
                base::FilePath(FILE_PATH_LITERAL("hello/index.html"))));

  EXPECT_TRUE(responses["https://test.example.org/index.html"]);
  EXPECT_TRUE(responses["https://test.example.org/manifest.webmanifest"]);
  EXPECT_TRUE(responses["https://test.example.org/script.js"]);
}

TEST_F(WebBundleParserTest, ParseSignedFile) {
  TestDataSource data_source(
      base::FilePath(FILE_PATH_LITERAL("hello_signed.wbn")));

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->authorities.size(), 1u);
  ASSERT_EQ(metadata->vouched_subsets.size(), 1u);
  EXPECT_EQ(metadata->vouched_subsets[0]->authority, 0u);

  const auto& parsed_signed = metadata->vouched_subsets[0]->parsed_signed;
  EXPECT_EQ(parsed_signed->validity_url, kValidityUrl);
  EXPECT_EQ(parsed_signed->date, kSignatureDate);
  EXPECT_EQ(parsed_signed->expires, kSignatureDate + kSignatureDuration);

  EXPECT_EQ(parsed_signed->subset_hashes.size(), metadata->requests.size());
  const auto& hashes =
      parsed_signed->subset_hashes[GURL("https://test.example.org/")];
  ASSERT_TRUE(hashes);
  EXPECT_EQ(hashes->variants_value, "");
  ASSERT_EQ(hashes->resource_integrities.size(), 1u);
  EXPECT_EQ(hashes->resource_integrities[0]->payload_integrity_header,
            "digest/mi-sha256-03");
}

// TODO(crbug.com/969596): Add a test case that loads a wbn file with variants,
// once gen-bundle supports variants.

}  // namespace web_package
