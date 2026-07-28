// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/timer/lap_timer.h"
#include "components/cbor/cbor_buildflags.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/web_package/web_bundle_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace cbor {
namespace {

constexpr char kMetricPrefixCBOR[] = "CBORReader.";
constexpr char kMetricTimePerParseUs[] = "parse_time";
constexpr char kMetricThroughputBps[] = "throughput";

// Benchmarking timer configuration constants:
// Payloads exceeding 1 MB use fewer warmup laps (3 laps vs 50 laps) to avoid
// multi-second delays per test iteration while still ensuring LapTimer
// stabilization.
constexpr size_t kLargePayloadThresholdBytes = 1'000'000;
constexpr int kDefaultWarmupLaps = 50;
constexpr int kLargePayloadWarmupLaps = 3;
constexpr base::TimeDelta kBenchmarkTimeLimit = base::Seconds(2);
constexpr int kTimerCheckIntervalLaps = 5;

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter(kMetricPrefixCBOR, story_name);
  reporter.RegisterImportantMetric(kMetricTimePerParseUs, "us");
  reporter.RegisterImportantMetric(kMetricThroughputBps, "bytes/s");
  return reporter;
}

std::vector<uint8_t> BuildFidoGetInfoResponsePayload() {
  cbor::Value::MapValue map;

  // Key 1: versions
  cbor::Value::ArrayValue versions;
  versions.emplace_back("FIDO_2_0");
  versions.emplace_back("FIDO_2_1");
  map.emplace(cbor::Value(1), cbor::Value(std::move(versions)));

  // Key 2: extensions
  cbor::Value::ArrayValue extensions;
  extensions.emplace_back("credBlob");
  extensions.emplace_back("hmac-secret");
  map.emplace(cbor::Value(2), cbor::Value(std::move(extensions)));

  // Key 3: aaguid
  map.emplace(cbor::Value(3), cbor::Value(std::vector<uint8_t>(16, 0x42)));

  // Key 4: options
  cbor::Value::MapValue options;
  options.emplace(cbor::Value("rk"), cbor::Value(true));
  options.emplace(cbor::Value("up"), cbor::Value(true));
  options.emplace(cbor::Value("uv"), cbor::Value(false));
  map.emplace(cbor::Value(4), cbor::Value(std::move(options)));

  // Key 5: maxMsgSize
  map.emplace(cbor::Value(5), cbor::Value(1024));

  // Key 6: pinProtocols
  cbor::Value::ArrayValue pin_protocols;
  pin_protocols.emplace_back(1);
  map.emplace(cbor::Value(6), cbor::Value(std::move(pin_protocols)));

  std::optional<std::vector<uint8_t>> serialized =
      Writer::Write(cbor::Value(std::move(map)));
  return CHECK_DEREF(std::move(serialized));
}

std::vector<uint8_t> BuildFidoPackedAttestationPayload() {
  cbor::Value::MapValue map;
  // Key "alg": COSE ES256 (-7)
  map.emplace(cbor::Value("alg"), cbor::Value(-7));
  // Key "fmt": "packed"
  map.emplace(cbor::Value("fmt"), cbor::Value("packed"));
  // Key "sig": signature bytes (64 bytes)
  map.emplace(cbor::Value("sig"), cbor::Value(std::vector<uint8_t>(64, 0xAA)));

  std::optional<std::vector<uint8_t>> serialized =
      Writer::Write(cbor::Value(std::move(map)));
  return CHECK_DEREF(std::move(serialized));
}

std::vector<uint8_t> BuildHugeAndComplexWebBundlePayload() {
  web_package::WebBundleBuilder builder;
  builder.AddPrimaryURL("https://chamber-door.example.com/");

  // Add custom metadata section for extra bundle complexity
  cbor::Value::MapValue metadata;
  metadata.emplace(cbor::Value("poem"), cbor::Value("the-raven"));
  metadata.emplace(cbor::Value("author"), cbor::Value("edgar-allan-poe"));
  metadata.emplace(cbor::Value("published"), cbor::Value(1845));
  metadata.emplace(cbor::Value("refrain"), cbor::Value("nevermore"));
  metadata.emplace(cbor::Value("atmosphere"),
                   cbor::Value("midnight-dreary-pondering (" +
                               std::string(4000, 'r') + ")"));
  builder.AddSection("raven-lore-metadata", cbor::Value(std::move(metadata)));

  constexpr std::string_view html_payload = R"(<!DOCTYPE html>
<html>
<head><title>Forgotten Lore</title></head>
<body><div class="bleak-december" id="midnight-pondering"></div></body>
</html>)";

  constexpr std::string_view json_payload = R"({
  "soul-status": "stronger",
  "hesitating": false,
  "visitor-action": "tapping",
  "visitor-location": "chamber-door"
})";

  // 16x16 RGBA PNG image of a pixel-art raven silhouette (152 bytes)
  constexpr std::string_view binary_payload(
      // PNG Header & IHDR Chunk (16x16, RGBA)
      "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52\x00"
      "\x00\x00\x10\x00\x00\x00\x10\x08\x06\x00\x00\x00\x1f\xf3\xff\x61"
      // IDAT Chunk (Compressed image data)
      "\x00\x00\x00\x5f\x49\x44\x41\x54\x78\xda\x63\x60\xa0\x15\xe0\xe4\xe4"
      "\xfe\x8f\x8c\xc9\xd2\x3c\x53\x5a\x1a\x6e\xc0\xd7\x79\x10\x4c\x96\xed"
      "\x24\xb9\x00\x9b\x66\xa2\x0d\x40\xd6\x20\x27\xa7\x0c\xc6\x44\x1b\x82"
      "\xae\x11\x19\x13\x34\x04\x9b\xcd\xb8\x0c\xc0\x6a\x08\xba\x02\x7c\x9a"
      "\x89\x72\x05\x3e\x4c\x56\x0c\x90\x1d\x13\x14\xa7\x44\xb2\x34\xa3\x1b"
      "\x42\xb2\x46\x1b\x1b\xc7\xff\x20\x8c\x8b\x4f\x75\x00\x00\x26\x98\x76"
      "\xbb\x73\x44\xa4\x5b"
      // IEND Chunk (End of PNG)
      "\x00\x00\x00\x00\x49\x45\x4e\x44\xae\x42\x60\x82");

  constexpr std::string_view js_payload =
      R"(const statelyRaven = {
  perch: 'bust-of-Pallas',
  location: 'above-chamber-door',
  origin: 'saintly-days-of-yore'
};)";

  constexpr std::string_view css_payload =
      R"(.ebony-bird {
  --countenance: grave-and-stern;
  --crest: shorn-and-shaven;
  --plutonian-shore: true;
})";

  // ~100 KB Plaintext payload: Infinite repetitions of "nevermore "
  const std::string text_payload =
      base::JoinString(std::vector<std::string_view>(10101, "nevermore"), " ");

  // Generate 1,260 diverse resources (21-22 MB total) across 6 distinct
  // domains and resource types
  for (int i = 0; i < 1260; ++i) {
    std::string url;
    web_package::WebBundleBuilder::Headers headers;
    std::string_view payload;

    int type = i % 6;
    std::string id_str = base::NumberToString(i);

    switch (type) {
      case 0:
        url = "https://chamber-door.example.com/pages/page_" + id_str + ".html";
        headers = {{":status", "200"},
                   {"content-type", "text/html; charset=utf-8"},
                   {"cache-control", "public, max-age=3600"},
                   {"etag", "\"lore_page_" + id_str + "\""},
                   {"x-content-type-options", "nosniff"},
                   {"x-frame-options", "SAMEORIGIN"},
                   {"referrer-policy", "strict-origin-when-cross-origin"}};
        payload = html_payload;
        break;
      case 1:
        url = "https://lore.example.com/v2/poe/data_" + id_str + ".json";
        headers = {{":status", "200"},
                   {"content-type", "application/json; charset=utf-8"},
                   {"cache-control", "no-cache, no-store, must-revalidate"},
                   {"vary", "Accept-Encoding, User-Agent"},
                   {"access-control-allow-origin", "*"},
                   {"x-api-version", "1845.1.0"},
                   {"x-raven-quote", "nevermore"}};
        payload = json_payload;
        break;
      case 2:
        url = "https://cdn.example.com/assets/images/raven_pixelart_" + id_str +
              ".png";
        headers = {{":status", "200"},
                   {"content-type", "image/png"},
                   {"cache-control", "public, max-age=31536000, immutable"},
                   {"etag", "\"raven_img_" + id_str + "\""},
                   {"content-disposition",
                    "inline; filename=\"raven_" + id_str + ".png\""},
                   {"accept-ranges", "bytes"}};
        payload = binary_payload;
        break;
      case 3:
        url = "https://static.example.com/js/raven/module_" + id_str + ".js";
        headers = {{":status", "200"},
                   {"content-type", "application/javascript; charset=utf-8"},
                   {"cache-control", "public, max-age=86400"},
                   {"content-security-policy", "default-src 'self'"},
                   {"strict-transport-security",
                    "max-age=31536000; includeSubDomains"}};
        payload = js_payload;
        break;
      case 4:
        url = "https://static.example.com/css/gothic/style_" + id_str + ".css";
        headers = {{":status", "200"},
                   {"content-type", "text/css; charset=utf-8"},
                   {"cache-control", "public, max-age=604800"},
                   {"x-custom-style-theme", "bleak_december_theme"}};
        payload = css_payload;
        break;
      case 5:
        url = "https://lore.example.com/texts/nevermore_" + id_str + ".txt";
        headers = {{":status", "200"},
                   {"content-type", "text/plain; charset=utf-8"},
                   {"cache-control", "public, max-age=3600"}};
        payload = text_payload;
        break;
    }

    builder.AddExchange(url, headers, payload);
  }

  return builder.CreateBundle();
}

class CBORReaderPerfTest : public testing::TestWithParam<bool> {
 protected:
  bool UseRust() const { return GetParam(); }

  void SetUp() override {
#if !BUILDFLAG(USE_CBOR_RUST)
    if (UseRust()) {
      GTEST_SKIP()
          << "Rust CBOR parser is disabled in this build configuration.";
    }
#endif
  }

  // Runs the CBOR reader performance benchmark for the current test case.
  void RunBenchmark(base::span<const uint8_t> payload,
                    size_t max_nesting_level = Reader::kCBORMaxDepth,
                    bool allow_invalid_utf8 = false) {
    // It's necessary to convert the parameter separator '/' to '_' (e.g.
    // "Synthetic_LargeStringArray_Cpp" vs "Synthetic_LargeStringArray_Rust").
    std::string story_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    std::replace(story_name.begin(), story_name.end(), '/', '_');

    size_t num_bytes_consumed = 0;
    Reader::DecoderError error_code = Reader::DecoderError::CBOR_NO_ERROR;

    Reader::Config config;
    config.use_rust = UseRust();
    config.max_nesting_level = max_nesting_level;
    config.allow_invalid_utf8 = allow_invalid_utf8;
    config.num_bytes_consumed = &num_bytes_consumed;
    config.error_code_out = &error_code;

    // Verify first that the payload parses cleanly.
    std::optional<Value> initial_parse = Reader::Read(payload, config);
    ASSERT_TRUE(initial_parse.has_value())
        << "Failed initial parse for story: " << story_name
        << ", error: " << static_cast<int>(error_code) << " ("
        << Reader::ErrorCodeToString(error_code) << ")";

    const int warmup_laps = payload.size() > kLargePayloadThresholdBytes
                                ? kLargePayloadWarmupLaps
                                : kDefaultWarmupLaps;
    base::LapTimer timer(warmup_laps, kBenchmarkTimeLimit,
                         kTimerCheckIntervalLaps);
    timer.Start();
    while (!timer.HasTimeLimitExpired()) {
      std::optional<Value> val = Reader::Read(payload, config);
      timer.NextLap();
    }

    perf_test::PerfResultReporter reporter = SetUpReporter(story_name);
    reporter.AddResult(kMetricTimePerParseUs, timer.TimePerLap());
    reporter.AddResult(
        kMetricThroughputBps,
        static_cast<double>(timer.LapsPerSecond()) * payload.size());
  }

  std::vector<uint8_t> LoadTestFile(const std::string& relative_path) {
    base::FilePath source_root;
    EXPECT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));
    return CHECK_DEREF(
        base::ReadFileToBytes(source_root.AppendASCII(relative_path)));
  }
};

// -----------------------------------------------------------------------------
// FIDO / WebAuthn CBOR Benchmarks
// -----------------------------------------------------------------------------

TEST_P(CBORReaderPerfTest, FIDO_PackedAttestation) {
  static const base::NoDestructor<std::vector<uint8_t>> payload(
      BuildFidoPackedAttestationPayload());
  RunBenchmark(*payload);
}

TEST_P(CBORReaderPerfTest, FIDO_GetInfoResponse) {
  static const base::NoDestructor<std::vector<uint8_t>> payload(
      BuildFidoGetInfoResponsePayload());
  RunBenchmark(*payload);
}

// -----------------------------------------------------------------------------
// Web Bundle (.wbn / .swbn) CBOR Benchmarks
// -----------------------------------------------------------------------------

TEST_P(CBORReaderPerfTest, WebBundle_SimpleB2) {
  std::vector<uint8_t> data =
      LoadTestFile("components/test/data/web_package/simple_b2.wbn");
  ASSERT_FALSE(data.empty());
  RunBenchmark(data);
}

TEST_P(CBORReaderPerfTest, WebBundle_HelloB2) {
  std::vector<uint8_t> data =
      LoadTestFile("components/test/data/web_package/hello_b2.wbn");
  ASSERT_FALSE(data.empty());
  RunBenchmark(data);
}

TEST_P(CBORReaderPerfTest, WebBundle_24Responses) {
  std::vector<uint8_t> data =
      LoadTestFile("components/test/data/web_package/24_responses.wbn");
  ASSERT_FALSE(data.empty());
  RunBenchmark(data);
}

TEST_P(CBORReaderPerfTest, SignedWebBundle_SimpleV2) {
  std::vector<uint8_t> data =
      LoadTestFile("components/test/data/web_package/simple_b2_signed_v2.swbn");
  ASSERT_FALSE(data.empty());
  RunBenchmark(data);
}

TEST_P(CBORReaderPerfTest, WebBundle_HugeWebBundle) {
  static const base::NoDestructor<std::vector<uint8_t>> payload(
      BuildHugeAndComplexWebBundlePayload());
  ASSERT_GT(payload->size(), 21504000u) << "Payload size should be > 21MB";
  ASSERT_LT(payload->size(), 22528000u) << "Payload size should be < 22MB";
  RunBenchmark(*payload);
}

// -----------------------------------------------------------------------------
// Synthetic Structural Benchmarks
// -----------------------------------------------------------------------------

TEST_P(CBORReaderPerfTest, Synthetic_LargeIntegerMap) {
  // Generate a CBOR map with 200 integer key-value pairs (canonically sorted).
  cbor::Value::MapValue map;
  for (int i = 0; i < 200; ++i) {
    map.emplace(cbor::Value(i), cbor::Value(i * 10));
  }
  cbor::Value root(std::move(map));
  std::optional<std::vector<uint8_t>> serialized = Writer::Write(root);
  ASSERT_TRUE(serialized.has_value());
  RunBenchmark(*serialized);
}

TEST_P(CBORReaderPerfTest, Synthetic_LargeStringArray) {
  // Generate a CBOR array with 200 string entries.
  cbor::Value::ArrayValue arr;
  for (int i = 0; i < 200; ++i) {
    arr.emplace_back("item_string_" + base::NumberToString(i));
  }
  cbor::Value root(std::move(arr));
  std::optional<std::vector<uint8_t>> serialized = Writer::Write(root);
  ASSERT_TRUE(serialized.has_value());
  RunBenchmark(*serialized);
}

TEST_P(CBORReaderPerfTest, Synthetic_DeeplyNestedArray) {
  // Generate a CBOR array nested 12 levels deep.
  cbor::Value current(12345);
  for (int i = 0; i < 12; ++i) {
    cbor::Value::ArrayValue arr;
    arr.emplace_back(std::move(current));
    current = cbor::Value(std::move(arr));
  }
  std::optional<std::vector<uint8_t>> serialized = Writer::Write(current, 16);
  ASSERT_TRUE(serialized.has_value());
  RunBenchmark(*serialized, 16);
}

INSTANTIATE_TEST_SUITE_P(,
                         CBORReaderPerfTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Rust" : "Cpp";
                         });

}  // namespace
}  // namespace cbor
