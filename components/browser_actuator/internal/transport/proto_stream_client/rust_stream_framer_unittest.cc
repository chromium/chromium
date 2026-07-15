// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport/proto_stream_client/rust_stream_framer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "components/browser_actuator/internal/transport/stream_framer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace browser_actuator {
namespace {

using testing::ElementsAre;

// The framing logic itself is exhaustively covered by the pure-Rust tests
// in rust/stream_body_parser_unittests.rs; these tests cover the FFI
// conversion into StreamFramer::FeedResult.

std::string Varint(uint64_t value) {
  std::string out;
  do {
    uint8_t byte = value & 0x7F;
    value >>= 7;
    if (value != 0) {
      byte |= 0x80;
    }
    out.push_back(static_cast<char>(byte));
  } while (value != 0);
  return out;
}

// Encodes one length-delimited StreamBody field.
std::string LenField(uint32_t field_number, std::string_view payload) {
  std::string out = Varint(field_number << 3 | 2);
  out += Varint(payload.size());
  out += payload;
  return out;
}

TEST(RustStreamFramerTest, FramesMessagesAcrossChunks) {
  RustStreamFramer framer;
  const std::string input = LenField(1, "one") + LenField(1, "two");

  StreamFramer::FeedResult first =
      framer.Feed(std::string_view(input).substr(0, input.size() - 1));
  EXPECT_THAT(first.messages, ElementsAre("one"));
  EXPECT_FALSE(first.status.has_value());
  EXPECT_FALSE(first.failed);

  StreamFramer::FeedResult second =
      framer.Feed(std::string_view(input).substr(input.size() - 1));
  EXPECT_THAT(second.messages, ElementsAre("two"));
  EXPECT_FALSE(second.failed);
}

TEST(RustStreamFramerTest, StatusIsDelivered) {
  RustStreamFramer framer;
  StreamFramer::FeedResult result =
      framer.Feed(LenField(1, "last message") + LenField(2, "rpc status"));
  EXPECT_THAT(result.messages, ElementsAre("last message"));
  EXPECT_EQ(result.status, "rpc status");
}

TEST(RustStreamFramerTest, EmptyStatusIsEngaged) {
  // A google.rpc.Status with code OK serializes to zero bytes; it must
  // arrive as an engaged, empty optional — not as nullopt.
  RustStreamFramer framer;
  StreamFramer::FeedResult result = framer.Feed(LenField(2, ""));
  EXPECT_TRUE(result.messages.empty());
  ASSERT_TRUE(result.status.has_value());
  EXPECT_TRUE(result.status->empty());
}

TEST(RustStreamFramerTest, NoopsAreInvisible) {
  RustStreamFramer framer;
  StreamFramer::FeedResult result =
      framer.Feed(LenField(15, "padding") + LenField(1, "real"));
  EXPECT_THAT(result.messages, ElementsAre("real"));
  EXPECT_FALSE(result.status.has_value());
  EXPECT_FALSE(result.failed);
}

TEST(RustStreamFramerTest, MalformedInputFailsTerminally) {
  RustStreamFramer framer;
  // Field 1 with the deprecated SGROUP wire type (tag 0x0B).
  EXPECT_TRUE(framer.Feed("\x0B").failed);

  StreamFramer::FeedResult after = framer.Feed(LenField(1, "well-formed"));
  EXPECT_TRUE(after.failed);
  EXPECT_TRUE(after.messages.empty());
}

TEST(RustStreamFramerTest, FactoryMintsIndependentFramers) {
  StreamFramerFactory factory = RustStreamFramer::MakeFactory();
  std::unique_ptr<StreamFramer> first = factory.Run();
  std::unique_ptr<StreamFramer> second = factory.Run();
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);

  // Half a message in one framer must not leak into the other.
  const std::string input = LenField(1, "message");
  EXPECT_TRUE(
      first->Feed(std::string_view(input).substr(0, 2)).messages.empty());
  EXPECT_THAT(second->Feed(input).messages, ElementsAre("message"));
  EXPECT_THAT(first->Feed(std::string_view(input).substr(2)).messages,
              ElementsAre("message"));
}

// Fuzzes the Rust StreamBody framing parser through the production
// StreamFramer surface (the cxx bridge and result conversion included).
// Taking a vector of chunks rather than one buffer hands the fuzzer
// direct control of chunk boundaries, exercising the incremental
// (cross-chunk) parsing paths.
void FeedDoesNotCrashOnArbitraryChunks(const std::vector<std::string>& chunks) {
  RustStreamFramer framer;
  for (const std::string& chunk : chunks) {
    if (framer.Feed(chunk).failed) {
      break;
    }
  }
}
// Seed corpus: complete, valid framings plus the format's interesting
// splits and hardening edges, so mutation starts deep inside the wire
// format instead of having to discover valid tag/length varints from
// scratch. Each seed is one chunk sequence, exactly as fed above.
std::vector<std::tuple<std::vector<std::string>>> StreamSeeds() {
  const std::string message = LenField(1, "seed message payload");
  const std::string status = LenField(2, "serialized rpc status");
  const std::string noop = LenField(15, "keep-alive");

  // One message delivered a byte at a time: every chunk boundary lands
  // mid-field, walking the parser's buffer-and-resume path.
  std::vector<std::string> byte_by_byte;
  for (char byte : message) {
    byte_by_byte.emplace_back(1, byte);
  }

  // A 300-byte payload needs a two-byte length varint; splitting at
  // offset 2 lands the boundary inside that varint.
  const std::string big = LenField(1, std::string(300, '\xAB'));

  // Fields this client does not know: field 3 varint (tag 0x18), field 4
  // fixed64 (0x21), field 5 fixed32 (0x2D), and a length-delimited
  // field 9 — all must be skipped, not delivered.
  const std::string unknowns = std::string(1, '\x18') + Varint(123456789) +
                               std::string(1, '\x21') + std::string(8, '\0') +
                               std::string(1, '\x2D') + std::string(4, '\0') +
                               LenField(9, "future StreamBody field");

  return {
      // A whole session in one chunk: messages, keep-alive, terminal
      // status.
      {{message + noop + LenField(1, "second message") + status}},
      {{message}},
      {byte_by_byte},
      {{big.substr(0, 2), big.substr(2)}},
      {{unknowns + message}},
      // Trailing fields after the terminal status still parse.
      {{status + message + noop}},
      // Empty chunks around an empty (but valid) message payload.
      {{"", LenField(1, ""), ""}},
      // The deprecated SGROUP wire type (tag 0x0B) after one good
      // message: the terminal-failure path.
      {{message + "\x0B"}},
      // A length claiming just over the parser's 10 MiB hardening limit,
      // with no payload behind it: the oversized-rejection path.
      {{Varint(1 << 3 | 2) + Varint(10 * 1024 * 1024 + 1)}},
  };
}

FUZZ_TEST(RustStreamFramerFuzzTest, FeedDoesNotCrashOnArbitraryChunks)
    .WithDomains(fuzztest::Arbitrary<std::vector<std::string>>())
    .WithSeeds(StreamSeeds);

}  // namespace
}  // namespace browser_actuator
