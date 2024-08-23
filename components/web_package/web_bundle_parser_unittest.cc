// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/web_package/web_bundle_parser.h"

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/cbor/writer.h"
#include "components/web_package/mojom/web_bundle_parser.mojom-forward.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/integrity_block_attributes.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/ed25519_key_pair.h"
#include "components/web_package/test_support/signed_web_bundles/web_bundle_signer.h"
#include "components/web_package/web_bundle_builder.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

constexpr char kPrimaryUrl[] = "https://test.example.com/";

std::string GetTestFileContents(const base::FilePath& path) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.Append(
      base::FilePath(FILE_PATH_LITERAL("components/test/data/web_package")));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(test_data_dir.Append(path), &contents));
  return contents;
}

class TestDataSource : public mojom::BundleDataSource {
 public:
  explicit TestDataSource(const base::FilePath& path,
                          const bool is_random_access_context = false)
      : data_(GetTestFileContents(path)),
        is_random_access_context_(is_random_access_context) {}
  explicit TestDataSource(const std::vector<uint8_t>& data,
                          const bool is_random_access_context = false)
      : data_(reinterpret_cast<const char*>(data.data()), data.size()),
        is_random_access_context_(is_random_access_context) {}

  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    if (offset >= data_.size()) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    const uint8_t* start =
        reinterpret_cast<const uint8_t*>(data_.data()) + offset;
    uint64_t available_length = std::min(length, data_.size() - offset);
    std::move(callback).Run(
        std::vector<uint8_t>(start, start + available_length));
  }

  void Length(LengthCallback callback) override {
    EXPECT_TRUE(is_random_access_context_);
    std::move(callback).Run(data_.size());
  }

  void IsRandomAccessContext(IsRandomAccessContextCallback callback) override {
    std::move(callback).Run(is_random_access_context_);
  }

  std::string_view GetPayload(const mojom::BundleResponsePtr& response) {
    return std::string_view(data_).substr(response->payload_offset,
                                          response->payload_length);
  }

  void AddReceiver(mojo::PendingReceiver<mojom::BundleDataSource> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void Close(CloseCallback callback) override {
    is_closed_ = true;
    std::move(callback).Run();
  }

  bool IsClosed() const { return is_closed_; }

 private:
  std::string data_;
  bool is_random_access_context_;
  mojo::ReceiverSet<mojom::BundleDataSource> receivers_;
  bool is_closed_ = false;
};

template <typename... T>
auto to_pair(std::tuple<T...>&& t)
    -> decltype(std::make_pair(std::get<0>(t), std::get<1>(t))) {
  return std::make_pair(std::move(std::get<0>(t)), std::move(std::get<1>(t)));
}

using ParseSignedBundleIntegrityBlockResult =
    base::expected<mojom::BundleIntegrityBlockPtr,
                   mojom::BundleIntegrityBlockParseErrorPtr>;

ParseSignedBundleIntegrityBlockResult ParseSignedBundleIntegrityBlock(
    TestDataSource* data_source,
    const GURL& base_url = GURL()) {
  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  data_source->AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

  WebBundleParser parser_impl(std::move(source_remote), base_url);
  mojom::WebBundleParser& parser = parser_impl;

  base::test::TestFuture<mojom::BundleIntegrityBlockPtr,
                         mojom::BundleIntegrityBlockParseErrorPtr>
      integrity_block_future;
  parser.ParseIntegrityBlock(integrity_block_future.GetCallback());
  auto parse_result = to_pair(integrity_block_future.Take());
  EXPECT_TRUE((parse_result.first && !parse_result.second) ||
              (!parse_result.first && parse_result.second));

  if (parse_result.first) {
    return std::move(parse_result.first);
  } else {
    return base::unexpected(std::move(parse_result.second));
  }
}

using ParseUnsignedBundleResult =
    std::pair<mojom::BundleMetadataPtr, mojom::BundleMetadataParseErrorPtr>;

ParseUnsignedBundleResult ParseUnsignedBundle(
    TestDataSource* data_source,
    const GURL& base_url = GURL(),
    std::optional<uint64_t> offset = std::nullopt) {
  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  data_source->AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

  WebBundleParser parser_impl(std::move(source_remote), base_url);
  mojom::WebBundleParser& parser = parser_impl;

  base::test::TestFuture<mojom::BundleMetadataPtr,
                         mojom::BundleMetadataParseErrorPtr>
      future;
  parser.ParseMetadata(offset, future.GetCallback());
  ParseUnsignedBundleResult result = to_pair(future.Take());
  EXPECT_TRUE((result.first && !result.second) ||
              (!result.first && result.second));
  return result;
}

void ExpectFormatError(ParseUnsignedBundleResult result) {
  ASSERT_TRUE(result.second);
  EXPECT_EQ(result.second->type, mojom::BundleParseErrorType::kFormatError);
}

// Finds the response for |url|.
mojom::BundleResponseLocationPtr FindResponse(
    const mojom::BundleMetadataPtr& metadata,
    const GURL& url) {
  const auto item = metadata->requests.find(url);
  if (item == metadata->requests.end()) {
    return nullptr;
  }

  return item->second.Clone();
}

mojom::BundleResponsePtr ParseResponse(
    TestDataSource* data_source,
    const mojom::BundleResponseLocationPtr& location,
    const GURL& base_url = GURL()) {
  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  data_source->AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

  WebBundleParser parser_impl(std::move(source_remote), base_url);
  mojom::WebBundleParser& parser = parser_impl;

  base::test::TestFuture<mojom::BundleResponsePtr,
                         mojom::BundleResponseParseErrorPtr>
      future;
  parser.ParseResponse(location->offset, location->length,
                       future.GetCallback());
  return std::get<0>(future.Take());
}

std::vector<uint8_t> CreateSmallBundle() {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kPrimaryUrl,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  return builder.CreateBundle();
}

struct SignedWebBundleAndKeys {
  std::vector<uint8_t> bundle;
  std::vector<test::KeyPair> key_pairs;
};

SignedWebBundleAndKeys SignBundle(
    const std::vector<uint8_t>& unsigned_bundle,
    std::optional<test::WebBundleSigner::IntegrityBlockAttributes>
        ib_attributes,
    test::WebBundleSigner::ErrorsForTesting errors_for_testing = {
        /*integrity_block_errors=*/{}, /*signatures_errors=*/{}},
    size_t num_signatures = 1) {
  std::vector<test::KeyPair> key_pairs;
  for (size_t i = 0; i < num_signatures; ++i) {
    key_pairs.push_back(test::Ed25519KeyPair::CreateRandom());
  }

  return {
      test::WebBundleSigner::SignBundle(unsigned_bundle, key_pairs,
                                        ib_attributes, errors_for_testing),
      key_pairs,
  };
}

SignedWebBundleAndKeys SignBundle(
    const std::vector<uint8_t>& unsigned_bundle,
    test::WebBundleSigner::ErrorsForTesting errors_for_testing = {
        /*integrity_block_errors=*/{}, /*signatures_errors=*/{}},
    size_t num_signatures = 1) {
  return SignBundle(unsigned_bundle, /*ib_attributes=*/{}, errors_for_testing,
                    num_signatures);
}

void CheckIfSignatureStackEntryIsValid(
    const mojom::BundleIntegrityBlockSignatureStackEntryPtr& entry,
    const Ed25519PublicKey& public_key) {
  ASSERT_TRUE(entry->signature_info->is_ed25519());
  EXPECT_EQ(entry->signature_info->get_ed25519()->public_key, public_key);

  // The attributes should contain the public key.
  EXPECT_NE(base::ranges::search(entry->attributes_cbor, public_key.bytes()),
            entry->attributes_cbor.end());
}

}  // namespace

using base::test::ErrorIs;
using testing::AllOf;
using testing::Eq;
using testing::Field;
using testing::HasSubstr;
using testing::Optional;
using testing::Pointee;
using testing::Property;

class WebBundleParserTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebBundleParserTest, WrongMagic) {
  WebBundleBuilder builder;
  std::vector<uint8_t> bundle = builder.CreateBundle();
  bundle[3] ^= 1;
  TestDataSource data_source(bundle);

  mojom::BundleMetadataParseErrorPtr error =
      ParseUnsignedBundle(&data_source).second;
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kFormatError);
}

TEST_F(WebBundleParserTest, UnknownVersion) {
  WebBundleBuilder builder;
  std::vector<uint8_t> bundle = builder.CreateBundle();
  // Modify the version string from "b2\0\0" to "q2\0\0".
  ASSERT_EQ(bundle[11], 'b');
  bundle[11] = 'q';
  TestDataSource data_source(bundle);

  mojom::BundleMetadataParseErrorPtr error =
      ParseUnsignedBundle(&data_source).second;
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kVersionError);
}

TEST_F(WebBundleParserTest, SectionLengthsTooLarge) {
  WebBundleBuilder builder;
  std::string too_long_section_name(8192, 'x');
  builder.AddSection(too_long_section_name, cbor::Value(0));
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseUnsignedBundle(&data_source));
}

TEST_F(WebBundleParserTest, DuplicateSectionName) {
  WebBundleBuilder builder;
  builder.AddSection("foo", cbor::Value(0));
  builder.AddSection("foo", cbor::Value(0));
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseUnsignedBundle(&data_source));
}

TEST_F(WebBundleParserTest, InvalidRequestURL) {
  WebBundleBuilder builder;
  builder.AddExchange("", {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseUnsignedBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLIsNotUTF8) {
  WebBundleBuilder builder(BundleVersion::kB2,
                           /*allow_invalid_utf8_strings_for_testing*/ true);
  builder.AddExchange("https://test.example.com/\xcc",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseUnsignedBundle(&data_source));
}

// TODO(crbug.com/40629011): Revisit this once
// https://github.com/WICG/webpackage/issues/468 is resolved.
TEST_F(WebBundleParserTest, RequestURLHasNonStandardScheme) {
  WebBundleBuilder builder;
  builder.AddExchange("foo://bar",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ASSERT_TRUE(ParseUnsignedBundle(&data_source).first);
}

TEST_F(WebBundleParserTest, RequestURLHasIsolatedAppScheme) {
  WebBundleBuilder builder;
  builder.AddExchange("isolated-app://foo",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ASSERT_TRUE(ParseUnsignedBundle(&data_source).first);
}

TEST_F(WebBundleParserTest, RequestURLHasCredentials) {
  WebBundleBuilder builder;
  builder.AddExchange("https://user:passwd@test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseUnsignedBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLHasFragment) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/#fragment",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseUnsignedBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLIsValidUuidInPackage) {
  const char uuid_in_package[] =
      "uuid-in-package:f81d4fae-7dec-11d0-a765-00a0c91e6bf6";
  WebBundleBuilder builder;
  builder.AddExchange(uuid_in_package,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 1u);
  auto location = FindResponse(metadata, GURL(uuid_in_package));
  ASSERT_TRUE(location);
}

TEST_F(WebBundleParserTest, NoStatusInResponseHeaders) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{"content-type", "text/plain"}},
                      "payload");  // ":status" is missing.
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, InvalidResponseStatus) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "0200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, ExtraPseudoInResponseHeaders) {
  WebBundleBuilder builder;
  builder.AddExchange(
      "https://test.example.com/",
      {{":status", "200"}, {":foo", ""}, {"content-type", "text/plain"}},
      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, UpperCaseCharacterInHeaderName) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"Content-Type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, InvalidHeaderValue) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "\n"}}, "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, NoContentTypeWithNonEmptyContent) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/", {{":status", "200"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, NoContentTypeWithEmptyContent) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/", {{":status", "301"}}, "");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_TRUE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, AllKnownSectionInCritical) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  cbor::Value::ArrayValue critical_section;
  critical_section.emplace_back("index");
  critical_section.emplace_back("critical");
  critical_section.emplace_back("responses");
  builder.AddSection("critical", cbor::Value(critical_section));
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);
}

TEST_F(WebBundleParserTest, UnknownSectionInCritical) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  cbor::Value::ArrayValue critical_section;
  critical_section.emplace_back("unknown_section_name");
  builder.AddSection("critical", cbor::Value(critical_section));
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseUnsignedBundle(&data_source));
}

TEST_F(WebBundleParserTest, ParseGoldenFile) {
  TestDataSource data_source(base::FilePath(FILE_PATH_LITERAL("hello_b2.wbn")));

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 4u);
  EXPECT_EQ(metadata->primary_url, "https://test.example.org/");

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

TEST_F(WebBundleParserTest, SingleEntry) {
  WebBundleBuilder builder;
  builder.AddPrimaryURL(kPrimaryUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->version, mojom::BundleFormatVersion::kB2);
  ASSERT_EQ(metadata->requests.size(), 1u);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
  EXPECT_EQ(metadata->primary_url, kPrimaryUrl);
}

TEST_F(WebBundleParserTest, NoPrimaryUrlSingleEntry) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
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
  EXPECT_FALSE(metadata->primary_url.has_value());
}

TEST_F(WebBundleParserTest, RelativeURL) {
  WebBundleBuilder builder;
  builder.AddPrimaryURL("path/to/primary_url");
  builder.AddExchange("path/to/file.txt",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  const GURL base_url("https://test.example.com/dir/test.wbn");
  mojom::BundleMetadataPtr metadata =
      ParseUnsignedBundle(&data_source, base_url).first;
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->primary_url,
            "https://test.example.com/dir/path/to/primary_url");
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 1u);
  auto location = FindResponse(
      metadata, GURL("https://test.example.com/dir/path/to/file.txt"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location, base_url);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
}

TEST_F(WebBundleParserTest, RandomAccessContextWithAutomaticOffset) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);

  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
}

TEST_F(WebBundleParserTest, RandomAccessContextWithFixedCorrectOffset) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  mojom::BundleMetadataPtr metadata =
      ParseUnsignedBundle(&data_source, GURL(), 0).first;
  ASSERT_TRUE(metadata);

  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
}

TEST_F(WebBundleParserTest, RandomAccessContextWithFixedIncorrectOffset) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  mojom::BundleMetadataParseErrorPtr error =
      ParseUnsignedBundle(&data_source, GURL(), 1).second;
  ASSERT_TRUE(error);
}

TEST_F(WebBundleParserTest,
       RandomAccessContextPrependedDataWithAutomaticOffset) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  bundle.insert(bundle.begin(),
                {'o', 't', 'h', 'e', 'r', ' ', 'd', 'a', 't', 'a'});
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  ASSERT_TRUE(metadata);

  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
}

TEST_F(WebBundleParserTest,
       RandomAccessContextPrependedDataWithFixedCorrectOffset) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  bundle.insert(bundle.begin(),
                {'o', 't', 'h', 'e', 'r', ' ', 'd', 'a', 't', 'a'});
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  mojom::BundleMetadataPtr metadata =
      ParseUnsignedBundle(&data_source, GURL(), 10).first;
  ASSERT_TRUE(metadata);

  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
}

TEST_F(WebBundleParserTest,
       RandomAccessContextPrependedDataWithFixedIncorrectOffset) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  bundle.insert(bundle.begin(),
                {'o', 't', 'h', 'e', 'r', ' ', 'd', 'a', 't', 'a'});
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  mojom::BundleMetadataParseErrorPtr error =
      ParseUnsignedBundle(&data_source, GURL(), 3).second;
  ASSERT_TRUE(error);
}

TEST_F(WebBundleParserTest, RandomAccessContextLengthSmallerThanWebBundle) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  std::vector<uint8_t> invalid_length = {0, 0, 0, 0, 0, 0, 0, 10};
  base::ranges::copy(invalid_length, bundle.end() - 8);
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  ExpectFormatError(ParseUnsignedBundle(&data_source));
}

TEST_F(WebBundleParserTest, RandomAccessContextFileSmallerThanLengthField) {
  std::vector<uint8_t> bundle = {1, 2, 3, 4};
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  ExpectFormatError(ParseUnsignedBundle(&data_source));
}

TEST_F(WebBundleParserTest, RandomAccessContextLengthBiggerThanFile) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  std::vector<uint8_t> invalid_length = {0xff, 0, 0, 0, 0, 0, 0, 0};
  base::ranges::copy(invalid_length, bundle.end() - 8);
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  ExpectFormatError(ParseUnsignedBundle(&data_source));
}

// TODO(crbug.com/40630324): Add a test case that loads a wbn file with
// variants, once gen-bundle supports variants.

// This test verifies that even if a bundle is signed, it is still readable as
// an unsigned bundle in random-access contexts, since the `length` field of the
// web bundle can be used to find the start of the unsigned bundle.
TEST_F(WebBundleParserTest, SignedBundleMetadataOnlyInRandomAccessContexts) {
  auto bundle_and_keys = SignBundle(CreateSmallBundle());
  TestDataSource data_source(bundle_and_keys.bundle, true);

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  EXPECT_TRUE(metadata);
}

// This test verifies that when a bundle is signed, it can not be read as an
// unsigned bundle in non-random-access contexts, since the `length` field of
// the web bundle can't be used then.
TEST_F(WebBundleParserTest, SignedBundleMetadataOnlyInNonRandomAccessContexts) {
  auto bundle_and_keys = SignBundle(CreateSmallBundle());
  TestDataSource data_source(bundle_and_keys.bundle, false);

  mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
  EXPECT_FALSE(metadata);
}

TEST_F(WebBundleParserTest, SignedBundleIntegrityBlockIsParsedCorrectly) {
  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys = SignBundle(unsigned_bundle);
  TestDataSource data_source(bundle_and_keys.bundle);

  ASSERT_OK_AND_ASSIGN(auto integrity_block,
                       ParseSignedBundleIntegrityBlock(&data_source));

  // The size of the integrity block should be exactly equal to the size
  // difference between a signed and an unsigned bundle.
  EXPECT_EQ(integrity_block->size,
            bundle_and_keys.bundle.size() - unsigned_bundle.size());

  // There should be exactly one signature stack entry, corresponding to the
  // public key that was used to sign the web bundle.
  EXPECT_EQ(integrity_block->signature_stack.size(), 1ul);
  auto& entry = integrity_block->signature_stack[0];
  EXPECT_NO_FATAL_FAILURE(CheckIfSignatureStackEntryIsValid(
      entry, absl::get<test::Ed25519KeyPair>(bundle_and_keys.key_pairs[0])
                 .public_key));
}

TEST_F(WebBundleParserTest,
       SignedBundleSignatureStackWithMultipleEntries_AllValid) {
  unsigned long num_signatures = base::RandInt(2, 15);

  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys =
      SignBundle(unsigned_bundle, /*errors_for_testing=*/
                 {/*integrity_block_errors=*/{}, /*signatures_errors=*/{}},
                 num_signatures);
  TestDataSource data_source(bundle_and_keys.bundle);

  ASSERT_OK_AND_ASSIGN(auto integrity_block,
                       ParseSignedBundleIntegrityBlock(&data_source));

  // The size of the integrity block should be exactly equal to the size
  // difference between a signed and an unsigned bundle.
  EXPECT_EQ(integrity_block->size,
            bundle_and_keys.bundle.size() - unsigned_bundle.size());

  // The signature stack should contain the expected number of signatures, and
  // each entry should correspond to the public key that was used to sign the
  // web bundle.
  EXPECT_EQ(integrity_block->signature_stack.size(), num_signatures);

  for (unsigned long i = 0; i < num_signatures; ++i) {
    EXPECT_NO_FATAL_FAILURE(CheckIfSignatureStackEntryIsValid(
        integrity_block->signature_stack[i],
        absl::get<test::Ed25519KeyPair>(bundle_and_keys.key_pairs[i])
            .public_key));
  }
}

TEST_F(WebBundleParserTest,
       SignedBundleSignatureStackWithMultipleEntries_SomeInvalid) {
  std::vector<test::WebBundleSigner::IntegritySignatureErrorsForTesting>
      signatures_errors = {
          {},
          {},
          {test::WebBundleSigner::IntegritySignatureErrorForTesting::
               kWrongSignatureStackEntryAttributeNameLength},
          {test::WebBundleSigner::IntegritySignatureErrorForTesting::
               kWrongSignatureStackEntryAttributeNameLength},
          {test::WebBundleSigner::IntegritySignatureErrorForTesting::
               kWrongSignatureStackEntryAttributeNameLength},
          {},
          {}};
  auto total_signatures = signatures_errors.size();

  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys = SignBundle(
      unsigned_bundle, {/*integrity_block_errors=*/{}, signatures_errors},
      total_signatures);
  TestDataSource data_source(bundle_and_keys.bundle);

  ASSERT_OK_AND_ASSIGN(auto integrity_block,
                       ParseSignedBundleIntegrityBlock(&data_source));

  // The size of the integrity block should be exactly equal to the size
  // difference between a signed and an unsigned bundle.
  EXPECT_EQ(integrity_block->size,
            bundle_and_keys.bundle.size() - unsigned_bundle.size());

  // The signature stack should contain all the signatures, with the invalid
  // signatures of type unknown and each valid entry should correspond to the
  // public key that was used to sign the web bundle.
  EXPECT_EQ(integrity_block->signature_stack.size(), total_signatures);

  for (size_t index = 0; index < total_signatures; ++index) {
    if (signatures_errors[index].empty()) {
      auto* key_pair =
          absl::get_if<test::Ed25519KeyPair>(&bundle_and_keys.key_pairs[index]);
      EXPECT_NO_FATAL_FAILURE(CheckIfSignatureStackEntryIsValid(
          integrity_block->signature_stack[index], key_pair->public_key));
    } else {
      EXPECT_TRUE(integrity_block->signature_stack[index]
                      ->signature_info->is_unknown());
    }
  }
}

TEST_F(WebBundleParserTest,
       SignedBundleSignatureStackWithMultipleEntries_FirstInvalid) {
  size_t total_signatures = 3;
  std::vector<test::WebBundleSigner::IntegritySignatureErrorsForTesting>
      signatures_errors(total_signatures);
  signatures_errors[0] = {
      test::WebBundleSigner::IntegritySignatureErrorForTesting::
          kWrongSignatureStackEntryAttributeNameLength};

  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys = SignBundle(
      unsigned_bundle, {/*integrity_block_errors=*/{}, signatures_errors},
      total_signatures);
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(ParseSignedBundleIntegrityBlock(&data_source),
              ErrorIs(Pointee(AllOf(
                  Field(&mojom::BundleIntegrityBlockParseError::type,
                        Eq(mojom::BundleParseErrorType::kFormatError)),
                  Field(&mojom::BundleIntegrityBlockParseError::message,
                        Eq("Unknown cipher type of the first signature."))))));
}

TEST_F(WebBundleParserTest, SignedBundleWrongMagic) {
  WebBundleBuilder builder;
  std::vector<uint8_t> unsigned_bundle = builder.CreateBundle();
  auto bundle_and_keys = SignBundle(unsigned_bundle);
  bundle_and_keys.bundle[3] ^= 1;
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(ParseSignedBundleIntegrityBlock(&data_source),
              ErrorIs(Pointee(
                  AllOf(Field(&mojom::BundleIntegrityBlockParseError::type,
                              Eq(mojom::BundleParseErrorType::kFormatError)),
                        Field(&mojom::BundleIntegrityBlockParseError::message,
                              Eq("Unexpected magic bytes."))))));
}

TEST_F(WebBundleParserTest, SignedBundleUnknownVersion) {
  WebBundleBuilder builder;
  std::vector<uint8_t> unsigned_bundle = builder.CreateBundle();
  auto bundle_and_keys = SignBundle(unsigned_bundle);
  // Modify the version string from "1b\0\0" to "1q\0\0".
  ASSERT_EQ(bundle_and_keys.bundle[12], 'b');
  bundle_and_keys.bundle[12] = 'q';
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(
      ParseSignedBundleIntegrityBlock(&data_source),
      ErrorIs(Pointee(Field(&mojom::BundleIntegrityBlockParseError::type,
                            Eq(mojom::BundleParseErrorType::kVersionError)))));
}

TEST_F(WebBundleParserTest, SignedBundleEmptySignatureStack) {
  std::vector<uint8_t> signed_bundle = test::WebBundleSigner::SignBundle(
      WebBundleBuilder().CreateBundle(), /*key_pairs=*/{}, /*ib_attributes=*/
      {{.web_bundle_id =
            "amoiebz32b7o24tilu257xne2yf3nkblkploanxzm7ebeglseqpfeaacai"}},
      {/*integrity_block_errors=*/{
           test::WebBundleSigner::IntegrityBlockErrorForTesting::
               kEmptySignatureList},
       /*signatures_errors=*/{}});
  TestDataSource data_source(signed_bundle);

  EXPECT_THAT(ParseSignedBundleIntegrityBlock(&data_source),
              ErrorIs(Pointee(
                  AllOf(Field(&mojom::BundleIntegrityBlockParseError::type,
                              Eq(mojom::BundleParseErrorType::kFormatError)),
                        Field(&mojom::BundleIntegrityBlockParseError::message,
                              Eq("The signature stack must contain at least "
                                 "one signature."))))));
}

TEST_F(WebBundleParserTest, SignedBundleNoBundleId) {
  WebBundleBuilder builder;
  std::vector<uint8_t> unsigned_bundle = builder.CreateBundle();
  auto bundle_and_keys =
      SignBundle(unsigned_bundle,
                 {/*integrity_block_errors=*/{
                      test::WebBundleSigner::IntegrityBlockErrorForTesting::
                          kNoSignedWebBundleId},
                  /*signatures_errors=*/{}});
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(ParseSignedBundleIntegrityBlock(&data_source),
              ErrorIs(Pointee(
                  AllOf(Field(&mojom::BundleIntegrityBlockParseError::type,
                              Eq(mojom::BundleParseErrorType::kFormatError)),
                        Field(&mojom::BundleIntegrityBlockParseError::message,
                              Eq("`webBundleId` field in integrity block "
                                 "attributes is missing or malformed."))))));
}

TEST_F(WebBundleParserTest, SignedBundleNoAttributes) {
  WebBundleBuilder builder;
  std::vector<uint8_t> unsigned_bundle = builder.CreateBundle();
  auto bundle_and_keys = SignBundle(
      unsigned_bundle,
      {/*integrity_block_errors=*/{
           test::WebBundleSigner::IntegrityBlockErrorForTesting::kNoAttributes},
       /*signatures_errors=*/{}});
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(
      ParseSignedBundleIntegrityBlock(&data_source),
      ErrorIs(Pointee(AllOf(
          Field(&mojom::BundleIntegrityBlockParseError::type,
                Eq(mojom::BundleParseErrorType::kFormatError)),
          Field(&mojom::BundleIntegrityBlockParseError::message,
                Eq("Integrity block array of length 3 - should be 4."))))));
}

TEST_F(WebBundleParserTest, SignedBundleWrongSignatureLength) {
  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys =
      SignBundle(unsigned_bundle,
                 {/*integrity_block_errors=*/{},
                  {{test::WebBundleSigner::IntegritySignatureErrorForTesting::
                        kInvalidSignatureLength}}});
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(ParseSignedBundleIntegrityBlock(&data_source),
              ErrorIs(Pointee(
                  AllOf(Field(&mojom::BundleIntegrityBlockParseError::type,
                              Eq(mojom::BundleParseErrorType::kFormatError)),
                        Field(&mojom::BundleIntegrityBlockParseError::message,
                              Eq("The signature has the wrong length. Expected "
                                 "64, but got 65 bytes."))))));
}

TEST_F(WebBundleParserTest, SignedBundleWrongSignatureStackEntryLength) {
  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys =
      SignBundle(unsigned_bundle,
                 {/*integrity_block_errors=*/{},
                  {{test::WebBundleSigner::IntegritySignatureErrorForTesting::
                        kAdditionalSignatureStackEntryElement}}});
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(ParseSignedBundleIntegrityBlock(&data_source),
              ErrorIs(Pointee(
                  AllOf(Field(&mojom::BundleIntegrityBlockParseError::type,
                              Eq(mojom::BundleParseErrorType::kFormatError)),
                        Field(&mojom::BundleIntegrityBlockParseError::message,
                              Eq("Each signature stack entry must contain "
                                 "exactly two elements."))))));
}

TEST_F(WebBundleParserTest, SignedBundleWithMultipleAttributes) {
  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys =
      SignBundle(unsigned_bundle,
                 {/*integrity_block_errors=*/{},
                  {{test::WebBundleSigner::IntegritySignatureErrorForTesting::
                        kAdditionalSignatureStackEntryAttributes}}});
  TestDataSource data_source(bundle_and_keys.bundle);

  ASSERT_OK_AND_ASSIGN(auto integrity_block,
                       ParseSignedBundleIntegrityBlock(&data_source));

  // The size of the integrity block should be exactly equal to the size
  // difference between a signed and an unsigned bundle.
  EXPECT_EQ(integrity_block->size,
            bundle_and_keys.bundle.size() - unsigned_bundle.size());

  // There should be exactly one signature stack entry, corresponding to the
  // public key that was used to sign the web bundle.
  EXPECT_EQ(integrity_block->signature_stack.size(), 1ul);
  auto& entry = integrity_block->signature_stack[0];
  EXPECT_NO_FATAL_FAILURE(CheckIfSignatureStackEntryIsValid(
      entry, absl::get<test::Ed25519KeyPair>(bundle_and_keys.key_pairs[0])
                 .public_key));
}

TEST_F(WebBundleParserTest, SignedBundleV2) {
  static constexpr std::string_view kWebBundleId =
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys = SignBundle(
      unsigned_bundle, {{.web_bundle_id = std::string(kWebBundleId)}},
      {/*integrity_block_errors=*/{},
       {{test::WebBundleSigner::IntegritySignatureErrorForTesting::
             kAdditionalSignatureStackEntryAttributes}}});
  TestDataSource data_source(bundle_and_keys.bundle);

  ASSERT_OK_AND_ASSIGN(auto integrity_block,
                       ParseSignedBundleIntegrityBlock(&data_source));

  // The size of the integrity block should be exactly equal to the size
  // difference between a signed and an unsigned bundle.
  EXPECT_EQ(integrity_block->size,
            bundle_and_keys.bundle.size() - unsigned_bundle.size());

  EXPECT_THAT(
      integrity_block->attributes,
      Property(&IntegrityBlockAttributes::web_bundle_id, Eq(kWebBundleId)));

  // There should be exactly one signature stack entry, corresponding to the
  // public key that was used to sign the web bundle.
  EXPECT_EQ(integrity_block->signature_stack.size(), 1ul);
  auto& entry = integrity_block->signature_stack[0];
  EXPECT_NO_FATAL_FAILURE(CheckIfSignatureStackEntryIsValid(
      entry, absl::get<test::Ed25519KeyPair>(bundle_and_keys.key_pairs[0])
                 .public_key));
}

TEST_F(WebBundleParserTest, SignedBundleWithMultiplePublicKeyAttributes) {
  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys =
      SignBundle(unsigned_bundle,
                 {/*integrity_block_errors=*/{},
                  {{test::WebBundleSigner::IntegritySignatureErrorForTesting::
                        kMultipleValidPublicKeyAttributes}}});
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(ParseSignedBundleIntegrityBlock(&data_source),
              ErrorIs(Pointee(AllOf(
                  Field(&mojom::BundleIntegrityBlockParseError::type,
                        Eq(mojom::BundleParseErrorType::kFormatError)),
                  Field(&mojom::BundleIntegrityBlockParseError::message,
                        Eq("Unknown cipher type of the first signature."))))));
}

TEST_F(WebBundleParserTest, SignedBundleUnsupportedSignatureAttributeMap) {
  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys =
      SignBundle(unsigned_bundle,
                 {/*integrity_block_errors=*/{},
                  {{test::WebBundleSigner::IntegritySignatureErrorForTesting::
                        kSignatureStackEntryUnsupportedMapAttribute}}});
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(
      ParseSignedBundleIntegrityBlock(&data_source),
      ErrorIs(Pointee(AllOf(
          Field(&mojom::BundleIntegrityBlockParseError::type,
                Eq(mojom::BundleParseErrorType::kFormatError)),
          Field(&mojom::BundleIntegrityBlockParseError::message,
                HasSubstr("nested attributes are currently not supported"))))));
}

TEST_F(WebBundleParserTest, SignedBundleUnsupportedSignatureAttributeArray) {
  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys =
      SignBundle(unsigned_bundle,
                 {/*integrity_block_errors=*/{},
                  {{test::WebBundleSigner::IntegritySignatureErrorForTesting::
                        kSignatureStackEntryUnsupportedArrayAttribute}}});
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(
      ParseSignedBundleIntegrityBlock(&data_source),
      ErrorIs(Pointee(AllOf(
          Field(&mojom::BundleIntegrityBlockParseError::type,
                Eq(mojom::BundleParseErrorType::kFormatError)),
          Field(&mojom::BundleIntegrityBlockParseError::message,
                HasSubstr("nested attributes are currently not supported"))))));
}

TEST_F(WebBundleParserTest, SignedBundleNoPublicKeyAttribute) {
  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys =
      SignBundle(unsigned_bundle,
                 {/*integrity_block_errors=*/{},
                  {{test::WebBundleSigner::IntegritySignatureErrorForTesting::
                        kNoPublicKeySignatureStackEntryAttribute}}});
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(ParseSignedBundleIntegrityBlock(&data_source),
              ErrorIs(Pointee(AllOf(
                  Field(&mojom::BundleIntegrityBlockParseError::type,
                        Eq(mojom::BundleParseErrorType::kFormatError)),
                  Field(&mojom::BundleIntegrityBlockParseError::message,
                        Eq("Unknown cipher type of the first signature."))))));
}

TEST_F(WebBundleParserTest, SignedBundleWrongPublicKeyAttributeName) {
  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys =
      SignBundle(unsigned_bundle,
                 {/*integrity_block_errors=*/{},
                  {{test::WebBundleSigner::IntegritySignatureErrorForTesting::
                        kWrongSignatureStackEntryAttributeName}}});
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(ParseSignedBundleIntegrityBlock(&data_source),
              ErrorIs(Pointee(AllOf(
                  Field(&mojom::BundleIntegrityBlockParseError::type,
                        Eq(mojom::BundleParseErrorType::kFormatError)),
                  Field(&mojom::BundleIntegrityBlockParseError::message,
                        Eq("Unknown cipher type of the first signature."))))));
}

TEST_F(WebBundleParserTest, SignedBundleWrongPublicKeyAttributeLength) {
  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys =
      SignBundle(unsigned_bundle,
                 {/*integrity_block_errors=*/{},
                  {{test::WebBundleSigner::IntegritySignatureErrorForTesting::
                        kWrongSignatureStackEntryAttributeNameLength}}});
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(ParseSignedBundleIntegrityBlock(&data_source),
              ErrorIs(Pointee(AllOf(
                  Field(&mojom::BundleIntegrityBlockParseError::type,
                        Eq(mojom::BundleParseErrorType::kFormatError)),
                  Field(&mojom::BundleIntegrityBlockParseError::message,
                        Eq("Unknown cipher type of the first signature."))))));
}

TEST_F(WebBundleParserTest, SignedBundleWrongPublicKeyLength) {
  auto unsigned_bundle = CreateSmallBundle();
  auto bundle_and_keys =
      SignBundle(unsigned_bundle,
                 {/*integrity_block_errors=*/{},
                  {{test::WebBundleSigner::IntegritySignatureErrorForTesting::
                        kInvalidPublicKeyLength}}});
  TestDataSource data_source(bundle_and_keys.bundle);

  EXPECT_THAT(ParseSignedBundleIntegrityBlock(&data_source),
              ErrorIs(Pointee(
                  AllOf(Field(&mojom::BundleIntegrityBlockParseError::type,
                              Eq(mojom::BundleParseErrorType::kFormatError)),
                        Field(&mojom::BundleIntegrityBlockParseError::message,
                              Eq("The Ed25519 public key does not have the "
                                 "correct length. Expected "
                                 "32 bytes, but received 33 bytes."))))));
}

TEST_F(WebBundleParserTest, DisconnectWhileParsingMetadata) {
  base::test::TestFuture<mojom::BundleMetadataPtr,
                         mojom::BundleMetadataParseErrorPtr>
      future;
  {
    WebBundleBuilder builder;
    builder.AddPrimaryURL(kPrimaryUrl);
    builder.AddExchange("https://test.example.com/",
                        {{":status", "200"}, {"content-type", "text/plain"}},
                        "payload");
    TestDataSource data_source(builder.CreateBundle());

    mojo::PendingRemote<mojom::BundleDataSource> source_remote;
    data_source.AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

    WebBundleParser parser_impl(std::move(source_remote), GURL());
    mojom::WebBundleParser& parser = parser_impl;

    parser.ParseMetadata(/*offset=*/std::nullopt, future.GetCallback());
    // |data_source| and |parser_impl| are deleted here.
  }

  auto error = std::get<1>(future.Take());
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kParserInternalError);
  EXPECT_EQ(error->message, "Data source disconnected.");
}

TEST_F(WebBundleParserTest, DisconnectWhileParsingResponse) {
  base::test::TestFuture<mojom::BundleResponsePtr,
                         mojom::BundleResponseParseErrorPtr>
      future;
  {
    WebBundleBuilder builder;
    builder.AddPrimaryURL(kPrimaryUrl);
    builder.AddExchange("https://test.example.com/",
                        {{":status", "200"}, {"content-type", "text/plain"}},
                        "payload");
    TestDataSource data_source(builder.CreateBundle());

    mojom::BundleMetadataPtr metadata = ParseUnsignedBundle(&data_source).first;
    ASSERT_TRUE(metadata);
    auto location = FindResponse(metadata, GURL("https://test.example.com/"));
    ASSERT_TRUE(location);

    mojo::PendingRemote<mojom::BundleDataSource> source_remote;
    data_source.AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

    WebBundleParser parser_impl(std::move(source_remote), GURL());
    mojom::WebBundleParser& parser = parser_impl;

    parser.ParseResponse(location->offset, location->length,
                         future.GetCallback());
    // |data_source| and |parser_impl| are deleted here.
  }

  auto error = std::get<1>(future.Take());
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kParserInternalError);
  EXPECT_EQ(error->message, "Data source disconnected.");
}

// This data source implementation never run result callback
// making the calls to it permanently pending.
class BlockingDataSource : public mojom::BundleDataSource {
 public:
  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {}
  void Length(LengthCallback callback) override {}
  void IsRandomAccessContext(IsRandomAccessContextCallback callback) override {}
  void Close(CloseCallback callback) override {}
};

TEST_F(WebBundleParserTest, DestructorWhileParsing) {
  base::test::TestFuture<mojom::BundleResponsePtr,
                         mojom::BundleResponseParseErrorPtr>
      response_future;
  base::test::TestFuture<mojom::BundleMetadataPtr,
                         mojom::BundleMetadataParseErrorPtr>
      metadata_future;
  base::test::TestFuture<mojom::BundleIntegrityBlockPtr,
                         mojom::BundleIntegrityBlockParseErrorPtr>
      integrity_block_future;

  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  mojo::MakeSelfOwnedReceiver(std::make_unique<BlockingDataSource>(),
                              source_remote.InitWithNewPipeAndPassReceiver());
  {
    WebBundleParser parser_impl(std::move(source_remote), GURL());
    mojom::WebBundleParser& parser = parser_impl;

    parser.ParseResponse(/*response_offset=*/100, /*response_length=*/1234,
                         response_future.GetCallback());
    parser.ParseMetadata(/*offset=*/std::nullopt,
                         metadata_future.GetCallback());
    parser.ParseIntegrityBlock(integrity_block_future.GetCallback());
    //|parser_impl| are deleted here.
  }

  {
    auto error_response = std::get<1>(response_future.Take());
    ASSERT_TRUE(error_response);
    EXPECT_EQ(error_response->type,
              mojom::BundleParseErrorType::kParserInternalError);
    EXPECT_EQ(error_response->message, "Data source disconnected.");
  }

  {
    auto error_metadata = std::get<1>(metadata_future.Take());
    ASSERT_TRUE(error_metadata);
    EXPECT_EQ(error_metadata->type,
              mojom::BundleParseErrorType::kParserInternalError);
    EXPECT_EQ(error_metadata->message, "Data source disconnected.");
  }

  {
    auto error_integrity_block = std::get<1>(integrity_block_future.Take());
    ASSERT_TRUE(error_integrity_block);
    EXPECT_EQ(error_integrity_block->type,
              mojom::BundleParseErrorType::kParserInternalError);
    EXPECT_EQ(error_integrity_block->message, "Data source disconnected.");
  }
}

TEST_F(WebBundleParserTest, Close) {
  auto unsigned_bundle = CreateSmallBundle();
  TestDataSource data_source(unsigned_bundle);
  EXPECT_FALSE(data_source.IsClosed());

  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  data_source.AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

  WebBundleParser parser_impl(std::move(source_remote), GURL());
  mojom::WebBundleParser& parser = parser_impl;

  base::test::TestFuture<void> future;
  parser.Close(future.GetCallback());
  future.Get();
  EXPECT_TRUE(data_source.IsClosed());
}

}  // namespace web_package
