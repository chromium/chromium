// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace cbor {

std::vector<std::tuple<std::vector<uint8_t>>> GetCborCorpus() {
  static base::NoDestructor<std::vector<std::tuple<std::vector<uint8_t>>>>
      seeds([] {
        base::FilePath source_root;
        // This should always be available in Chromium tests. If it fails,
        // we want to crash rather than silently run the fuzzer with an empty
        // corpus.
        CHECK(
            base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root))
            << "Failed to get DIR_SRC_TEST_DATA_ROOT.";

        const auto seeds = base::ToVector(
            fuzztest::ReadFilesFromDirectory(
                source_root.AppendASCII("components")
                    .AppendASCII("cbor")
                    .AppendASCII("reader_fuzzer_corpus")
                    .MaybeAsASCII()),
            [](const auto& seed) {
              return std::tuple{base::ToVector<uint8_t>(std::get<0>(seed))};
            });
        CHECK(!seeds.empty()) << "Seeds not found!";
        return seeds;
      }());
  return *seeds;
}

std::vector<std::tuple<std::vector<uint8_t>, bool, bool, int>>
GetCborCorpusWithConfig() {
  return base::ToVector(GetCborCorpus(), [](const auto& seed) {
    // Provide default valid values for the config parameters alongside the
    // seed.
    return std::tuple(std::get<0>(seed), false, false,
                      int{cbor::Reader::kCBORMaxDepth});
  });
}

struct CborAST;
using CborASTArray = std::vector<CborAST>;
using CborASTMap = std::vector<std::pair<CborAST, CborAST>>;

struct CborAST {
  std::variant<int64_t,
               std::string,
               std::vector<uint8_t>,
               CborASTArray,
               CborASTMap>
      val;
};

CborAST CborASTMapKey(std::variant<int64_t, std::string> v) {
  return std::visit([](auto&& arg) { return CborAST{std::move(arg)}; },
                    std::move(v));
}

// Generates a CborAST which represents a valid, randomized CBOR structure.
// This is used by ParseStructuredCBORDoesNotCrash to test the parser against
// deeply nested, complex, but structurally valid CBOR inputs.
// Constraints:
// - Strings are limited to printable ASCII to avoid generating invalid UTF-8
//   which would intentionally fail a DCHECK inside cbor::Value.
// - Sizes for arrays and maps are constrained to min_size=1 and max_size=3.
//   In testing various numbers, this was the best configuration to achieve a
//   good distribution of both deep and shallow trees, natively hitting
//   depths > 16 around 0.5% of the time without overflowing the C++ stack
//   or getting stuck in very deep trees.
fuzztest::Domain<CborAST> ArbitraryCborAST() {
  fuzztest::DomainBuilder builder;
  builder.Set<CborAST>(
      "value",
      fuzztest::Map(
          [](auto v) { return CborAST{std::move(v)}; },
          fuzztest::VariantOf(
              fuzztest::Arbitrary<int64_t>(), fuzztest::PrintableAsciiString(),
              fuzztest::Arbitrary<std::vector<uint8_t>>(),
              fuzztest::ContainerOf<CborASTArray>(builder.Get<CborAST>("value"))
                  .WithMinSize(1)
                  .WithMaxSize(3),
              fuzztest::ContainerOf<CborASTMap>(
                  fuzztest::PairOf(
                      fuzztest::Map(&CborASTMapKey,
                                    fuzztest::VariantOf(
                                        fuzztest::Arbitrary<int64_t>(),
                                        fuzztest::PrintableAsciiString())),
                      builder.Get<CborAST>("value")))
                  .WithMinSize(1)
                  .WithMaxSize(3))));
  return std::move(builder).Finalize<CborAST>("value");
}

cbor::Value ASTToCborValue(const CborAST& ast) {
  return std::visit(
      absl::Overload{[](const CborASTArray& v) {
                       return cbor::Value(base::ToVector(v, ASTToCborValue));
                     },
                     [](const CborASTMap& v) {
                       return cbor::Value(cbor::Value::MapValue(
                           base::ToVector(v, [](const auto& pair) {
                             return std::pair(ASTToCborValue(pair.first),
                                              ASTToCborValue(pair.second));
                           })));
                     },
                     [](const auto& v) { return cbor::Value(v); }},
      ast.val);
}

std::vector<uint8_t> SerializeCborAST(const CborAST& ast) {
  // Pass a large max_nesting_level (999) to ensure Writer doesn't arbitrarily
  // reject deep trees generated by the fuzzer that exceed the default 16.
  std::optional<std::vector<uint8_t>> serialized_cbor =
      Writer::Write(ASTToCborValue(ast), 999);
  CHECK(serialized_cbor);
  return *std::move(serialized_cbor);
}

fuzztest::Domain<std::vector<uint8_t>> ArbitrarySerializedCbor() {
  return fuzztest::Map(&SerializeCborAST, ArbitraryCborAST());
}

void ReadAndWriteIsIdempotentAndDoesNotCrash(
    const std::vector<uint8_t>& input) {
  std::optional<Value> cbor = Reader::Read(input);
  if (cbor) {
    std::optional<std::vector<uint8_t>> serialized_cbor = Writer::Write(*cbor);
    CHECK(serialized_cbor);
    // This can only be reached if the input was canonical, which means that it
    // must exactly match the re-serialized output.
    CHECK(*serialized_cbor == input);
  }
}

// Tests that the CBOR reader handles valid, canonical inputs correctly and
// safely rejects invalid/random bytes without crashing. This is fed by random
// (but seeded) binary data.
FUZZ_TEST(CBORReaderFuzzTest, ReadAndWriteIsIdempotentAndDoesNotCrash)
    .WithDomains(fuzztest::Arbitrary<std::vector<uint8_t>>())
    .WithSeeds(GetCborCorpus);

void ReadWithConfigDoesNotCrash(const std::vector<uint8_t>& input,
                                bool allow_invalid_utf8,
                                bool allow_floating_point,
                                int max_nesting_level) {
  Reader::Config config;
  config.allow_invalid_utf8 = allow_invalid_utf8;
  config.allow_floating_point = allow_floating_point;
  config.max_nesting_level = max_nesting_level;

  Reader::Read(input, config);
}

// Similar to ParseDoesNotCrash, but explores the parser's resilience when
// non-default configuration is set.
FUZZ_TEST(CBORReaderFuzzTest, ReadWithConfigDoesNotCrash)
    .WithDomains(fuzztest::Arbitrary<std::vector<uint8_t>>(),
                 fuzztest::Arbitrary<bool>(),
                 fuzztest::Arbitrary<bool>(),
                 fuzztest::InRange(0, int{cbor::Reader::kCBORMaxDepth}))
    .WithSeeds(GetCborCorpusWithConfig);

void ReadValidCBORWithConfigDoesNotCrash(const std::vector<uint8_t>& input,
                                         bool allow_invalid_utf8,
                                         bool allow_floating_point,
                                         int max_nesting_level) {
  ReadWithConfigDoesNotCrash(input, allow_invalid_utf8, allow_floating_point,
                             max_nesting_level);
}

// Similar to ParseWithConfigDoesNotCrash, but instead of mutating predefined
// seeds feeds always valid CBOR.
FUZZ_TEST(CBORReaderFuzzTest, ReadValidCBORWithConfigDoesNotCrash)
    .WithDomains(ArbitrarySerializedCbor(),
                 fuzztest::Arbitrary<bool>(),
                 fuzztest::Arbitrary<bool>(),
                 fuzztest::InRange(0, int{cbor::Reader::kCBORMaxDepth}));

void ReadValidAndTruncatedCBORDoesNotCrash(const std::vector<uint8_t>& input,
                                           size_t truncate_amount) {
  Reader::DecoderError error;
  std::optional<Value> cbor = Reader::Read(input, &error);

  CHECK(cbor || error == Reader::DecoderError::TOO_MUCH_NESTING);

  // Also test truncation resilience with varying amounts.
  size_t split_point = truncate_amount % input.size();
  if (split_point > 0) {
    auto [first, rest] = base::span(input).split_at(split_point);
    Reader::DecoderError truncate_error;
    Reader::Read(first, &truncate_error);
    Reader::Read(rest, &truncate_error);
  }
}

// Specifically tests the parser's robustness when parsing complex CBOR data
// that has been abruptly truncated. This simulates transmission errors or
// cut-off streams on valid payloads.
FUZZ_TEST(CBORReaderFuzzTest, ReadValidAndTruncatedCBORDoesNotCrash)
    .WithDomains(ArbitrarySerializedCbor(), fuzztest::Arbitrary<size_t>());

}  // namespace cbor
