// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/url_fixer.h"

#include <stdint.h>

#include <string>
#include <string_view>
#include <tuple>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "url/third_party/mozilla/url_parse.h"

namespace url_formatter {
namespace {

base::FilePath GenerateFuzzedFilePath(std::string_view valid_utf8_string) {
#if BUILDFLAG(IS_WIN)
  return base::FilePath(base::UTF8ToWide(valid_utf8_string));
#else
  return base::FilePath(valid_utf8_string);
#endif
}

// Theoretically, FuzzTest should be able to apply `.WithMaxSize()`
// onto the UTF-8 strings domain, but not right this moment.
static constexpr size_t kMaxFuzzerInputBytes = 100 * 1024;

// Initializes ICU tables for functions that require them.
class URLFixerFuzzer {
 public:
  URLFixerFuzzer() {
    base::i18n::AllowMultipleInitializeCallsForTesting();
    CHECK(base::i18n::InitializeICU());
  }
  ~URLFixerFuzzer() = default;

  void FuzzSegmentURL(std::string_view valid_utf8_string) {
    if (valid_utf8_string.length() > kMaxFuzzerInputBytes) {
      return;
    }
    url::Parsed parts;
    std::ignore = url_formatter::SegmentURL(valid_utf8_string, &parts);
  }

  void FuzzUTF16SegmentURL(std::string_view valid_utf8_string) {
    if (valid_utf8_string.length() > kMaxFuzzerInputBytes) {
      return;
    }
    url::Parsed parts;
    std::ignore =
        url_formatter::SegmentURL(base::UTF8ToUTF16(valid_utf8_string), &parts);
  }

  void FuzzFormatURL(std::string_view first_valid_utf8_string,
                     url_formatter::FormatUrlType format_url_type,
                     base::UnescapeRule::Type unescape_rule_type) {
    url::Parsed parsed;
    GURL unparsed(first_valid_utf8_string);
    url_formatter::FormatUrl(unparsed, format_url_type, unescape_rule_type,
                             &parsed, nullptr, nullptr);
  }

  void FuzzFixupURL(const std::string& first_valid_utf8_string,
                    const std::string& string_not_beginning_with_dot) {
    if (first_valid_utf8_string.length() > kMaxFuzzerInputBytes ||
        string_not_beginning_with_dot.length() > kMaxFuzzerInputBytes) {
      return;
    }

    std::ignore = url_formatter::FixupURL(first_valid_utf8_string,
                                          string_not_beginning_with_dot);
  }

  void FuzzFixupRelativeFile(std::string_view first_valid_utf8_string,
                             std::string_view second_valid_utf8_string) {
    if (first_valid_utf8_string.length() > kMaxFuzzerInputBytes ||
        second_valid_utf8_string.length() > kMaxFuzzerInputBytes) {
      return;
    }

    std::ignore = url_formatter::FixupRelativeFile(
        GenerateFuzzedFilePath(first_valid_utf8_string),
        GenerateFuzzedFilePath(second_valid_utf8_string));
  }
};

// Given the highest bit of an enum-ish bitflag, calculates the "max"
// value of the bitflag.
// Toy example: given `0b100`, the "maximal bitflag" would be `0b111`.
// Assumes without enforcing that `max_enum_bit` is a power of 2.
constexpr uint32_t MaxBitflagOf(uint32_t max_enum_bit) {
  return (max_enum_bit << 1) - 1;
}

}  // namespace

FUZZ_TEST_F(URLFixerFuzzer, FuzzSegmentURL).WithDomains(fuzztest::Utf8String());
FUZZ_TEST_F(URLFixerFuzzer, FuzzUTF16SegmentURL)
    .WithDomains(fuzztest::Utf8String());

FUZZ_TEST_F(URLFixerFuzzer, FuzzFormatURL)
    .WithDomains(
        fuzztest::Utf8String(),
        fuzztest::InRange(
            0u,
            MaxBitflagOf(url_formatter::kFormatUrlOmitMobilePrefix)),
        fuzztest::InRange(
            0u,
            MaxBitflagOf(base::UnescapeRule::REPLACE_PLUS_WITH_SPACE)));

// `AddDesiredTLD()` will `DCHECK` that the TLD does _not_ begin
// with `.`.
FUZZ_TEST_F(URLFixerFuzzer, FuzzFixupURL)
    .WithDomains(fuzztest::Utf8String(), fuzztest::InRegexp("^[^.].+"));

FUZZ_TEST_F(URLFixerFuzzer, FuzzFixupRelativeFile)
    .WithDomains(fuzztest::Utf8String(), fuzztest::Utf8String());

}  // namespace url_formatter
