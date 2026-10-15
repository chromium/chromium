// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "components/cbor/cbor_buildflags.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace cbor {

namespace {

std::optional<Value> ParseAndCompare(
    const base::span<const uint8_t> input,
    const Reader::Config* config_ptr = nullptr) {
  const auto fill_config = [&](Reader::Config& cfg, bool use_rust,
                               Reader::DecoderError* error_out) {
    if (config_ptr) {
      cfg.allow_invalid_utf8 = config_ptr->allow_invalid_utf8;
      cfg.max_nesting_level = config_ptr->max_nesting_level;
    }
    cfg.use_rust = use_rust;
    cfg.error_code_out = error_out;
  };

  Reader::Config cpp_config;
  Reader::DecoderError cpp_error;
  fill_config(cpp_config, false, &cpp_error);
  std::optional<Value> cpp_cbor = Reader::Read(input, cpp_config);

#if BUILDFLAG(USE_CBOR_RUST)
  Reader::Config rust_config;
  Reader::DecoderError rust_error;
  fill_config(rust_config, true, &rust_error);
  std::optional<Value> rust_cbor = Reader::Read(input, rust_config);

  EXPECT_EQ(rust_cbor, cpp_cbor);
  if (cpp_cbor.has_value() && rust_cbor.has_value()) {
    Writer::Config cpp_writer_config;
    cpp_writer_config.use_rust = false;
    Writer::Config rust_writer_config;
    rust_writer_config.use_rust = true;
    if (config_ptr) {
      cpp_writer_config.allow_invalid_utf8_for_testing =
          config_ptr->allow_invalid_utf8;
      rust_writer_config.allow_invalid_utf8_for_testing =
          config_ptr->allow_invalid_utf8;
    }
    std::optional<std::vector<uint8_t>> cpp_out =
        Writer::Write(*cpp_cbor, cpp_writer_config);
    std::optional<std::vector<uint8_t>> rust_out =
        Writer::Write(*rust_cbor, rust_writer_config);
    EXPECT_EQ(rust_out, cpp_out);
  } else {
    // Both parsers correctly rejected the invalid input. Check that the error
    // codes align perfectly.
    EXPECT_EQ(rust_error, cpp_error);
  }
#endif

  if (config_ptr && config_ptr->error_code_out) {
    *config_ptr->error_code_out = cpp_error;
  }
  return cpp_cbor;
}

const std::vector<std::string>& GetCborCorpus() {
  static const base::NoDestructor<std::vector<std::string>> seeds([] {
    base::FilePath source_root;
    // This should always be available in Chromium tests. If it fails,
    // we want to crash rather than silently run the fuzzer with an empty
    // corpus.
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root))
        << "Failed to get DIR_SRC_TEST_DATA_ROOT.";

    auto seeds = base::ToVector(fuzztest::ReadFilesFromDirectory(
                                    source_root.AppendASCII("components")
                                        .AppendASCII("cbor")
                                        .AppendASCII("reader_fuzzer_corpus")
                                        .MaybeAsASCII()),
                                [](std::tuple<std::string>& seed) {
                                  return std::move(std::get<0>(seed));
                                });
    CHECK(!seeds.empty()) << "Seeds not found!";
    return seeds;
  }());
  return *seeds;
}

// Generates a valid, randomized CBOR Value to test the parser against deeply
// nested, complex, but structurally valid CBOR inputs.
// Constraints:
// - Strings are limited to valid UTF-8 to avoid failing a CHECK inside
//   cbor::Value.
// - Sizes for arrays and maps are constrained to max_size=3. In testing various
//   numbers, this was the best configuration to achieve a good distribution of
//   both deep and shallow trees without overflowing the C++ stack or getting
//   stuck in very deep trees.
fuzztest::Domain<Value> ArbitraryCborValue() {
  fuzztest::DomainBuilder builder;
  builder.Set<Value>(
      "value",
      fuzztest::OneOf(
          fuzztest::ConstructorOf<Value>(fuzztest::Arbitrary<int64_t>()),
          fuzztest::ConstructorOf<Value>(fuzztest::Utf8String()),
          fuzztest::ConstructorOf<Value>(
              fuzztest::Arbitrary<Value::BinaryValue>()),
          fuzztest::ConstructorOf<Value>(fuzztest::Arbitrary<bool>()),
          fuzztest::ConstructorOf<Value>(fuzztest::Just(Value::null)),
          fuzztest::ConstructorOf<Value>(fuzztest::Just(Value::undefined)),
          fuzztest::ConstructorOf<Value>(
              fuzztest::ContainerOf<Value::ArrayValue>(
                  builder.Get<Value>("value"))
                  .WithMaxSize(3)),
          fuzztest::ConstructorOf<Value>(
              fuzztest::ContainerOf<Value::MapValue>(
                  fuzztest::PairOf(
                      fuzztest::OneOf(
                          fuzztest::ConstructorOf<Value>(
                              fuzztest::Arbitrary<int64_t>()),
                          fuzztest::ConstructorOf<Value>(
                              fuzztest::Utf8String()),
                          fuzztest::ConstructorOf<Value>(
                              fuzztest::Arbitrary<Value::BinaryValue>())),
                      builder.Get<Value>("value")))
                  .WithMaxSize(3))));
  return std::move(builder).Finalize<Value>("value");
}

std::vector<uint8_t> SerializeCborValue(ValueView val) {
  // Pass a large max_nesting_level (999) to ensure Writer doesn't arbitrarily
  // reject deep trees generated by the fuzzer that exceed the default 16.
  Writer::Config cpp_config;
  cpp_config.max_nesting_level = 999;
  cpp_config.use_rust = false;
  std::optional<std::vector<uint8_t>> serialized_cbor =
      Writer::Write(val, cpp_config);
  CHECK(serialized_cbor);

#if BUILDFLAG(USE_CBOR_RUST)
  Writer::Config rust_config;
  rust_config.max_nesting_level = 999;
  rust_config.use_rust = true;
  std::optional<std::vector<uint8_t>> rust_serialized =
      Writer::Write(val, rust_config);
  CHECK(rust_serialized);
  CHECK(*rust_serialized == *serialized_cbor);
#endif

  return *std::move(serialized_cbor);
}

fuzztest::Domain<std::vector<uint8_t>> ArbitrarySerializedCbor() {
  return fuzztest::Map(&SerializeCborValue, ArbitraryCborValue());
}

void ReadAndWriteIsIdempotentAndDoesNotCrash(const std::string& input) {
  const base::span<const uint8_t> input_bytes = base::as_byte_span(input);
  std::optional<Value> cbor = ParseAndCompare(input_bytes);
  if (cbor) {
    std::optional<std::vector<uint8_t>> serialized_cbor = Writer::Write(*cbor);
    ASSERT_TRUE(serialized_cbor.has_value());
    // This can only be reached if the input was canonical, which means that it
    // must exactly match the re-serialized output.
    EXPECT_EQ(*serialized_cbor, input_bytes);
  }
}

// Tests that the CBOR reader handles valid, canonical inputs correctly and
// safely rejects invalid/random bytes without crashing. This is fed by random
// (but seeded) binary data.
FUZZ_TEST(CBORReaderFuzzTest, ReadAndWriteIsIdempotentAndDoesNotCrash)
    .WithDomains(fuzztest::Arbitrary<std::string>().WithSeeds(GetCborCorpus));

void ReadWithConfig(base::span<const uint8_t> input,
                    bool allow_invalid_utf8,
                    int max_nesting_level) {
  Reader::Config config;
  config.allow_invalid_utf8 = allow_invalid_utf8;
  config.max_nesting_level = max_nesting_level;

  ParseAndCompare(input, &config);
}

void ReadWithConfigDoesNotCrash(const std::string& input,
                                bool allow_invalid_utf8,
                                int max_nesting_level) {
  ReadWithConfig(base::as_byte_span(input), allow_invalid_utf8,
                 max_nesting_level);
}

// Similar to ReadAndWriteIsIdempotentAndDoesNotCrash, but explores the parser's
// resilience when non-default configuration is set.
FUZZ_TEST(CBORReaderFuzzTest, ReadWithConfigDoesNotCrash)
    .WithDomains(fuzztest::Arbitrary<std::string>().WithSeeds(GetCborCorpus),
                 fuzztest::Arbitrary<bool>(),
                 fuzztest::InRange(0, int{cbor::Reader::kCBORMaxDepth}));

void ReadValidCBORWithConfigDoesNotCrash(const std::vector<uint8_t>& input,
                                         bool allow_invalid_utf8,
                                         int max_nesting_level) {
  ReadWithConfig(input, allow_invalid_utf8, max_nesting_level);
}

// Similar to ReadWithConfigDoesNotCrash, but instead of mutating predefined
// seeds feeds always valid CBOR.
FUZZ_TEST(CBORReaderFuzzTest, ReadValidCBORWithConfigDoesNotCrash)
    .WithDomains(ArbitrarySerializedCbor(),
                 fuzztest::Arbitrary<bool>(),
                 fuzztest::InRange(0, int{cbor::Reader::kCBORMaxDepth}));

void ReadValidAndTruncatedCBORDoesNotCrash(const std::vector<uint8_t>& input,
                                           size_t truncate_amount) {
  Reader::Config config;
  Reader::DecoderError error;
  config.error_code_out = &error;
  std::optional<Value> cbor = ParseAndCompare(input, &config);

  ASSERT_TRUE(cbor.has_value() ||
              error == Reader::DecoderError::TOO_MUCH_NESTING);

  // Also test truncation resilience with varying amounts.
  size_t split_point = truncate_amount % input.size();
  if (split_point > 0) {
    auto [first, rest] = base::span(input).split_at(split_point);
    Reader::Config truncate_config;
    Reader::DecoderError truncate_error;
    truncate_config.error_code_out = &truncate_error;
    ParseAndCompare(first, &truncate_config);
    ParseAndCompare(rest, &truncate_config);
  }
}

// Specifically tests the parser's robustness when parsing complex CBOR data
// that has been abruptly truncated. This simulates transmission errors or
// cut-off streams on valid payloads.
FUZZ_TEST(CBORReaderFuzzTest, ReadValidAndTruncatedCBORDoesNotCrash)
    .WithDomains(ArbitrarySerializedCbor(), fuzztest::Arbitrary<size_t>());

}  // namespace
}  // namespace cbor
