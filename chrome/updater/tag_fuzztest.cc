// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/tag_internal.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace updater::tagging {

namespace {

constexpr std::string_view kGUIDRegex =
    "[0-9a-fA-F]{8}-([0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}";

struct SharedInput {
  std::string tag;
  std::optional<std::string> app_installer_data_args;
};

void FuzzParse(std::string tag,
               std::optional<std::string> app_installer_data_args) {
  TagArgs args;
  std::optional<std::string_view> app_installer_data_args_view;
  if (app_installer_data_args) {
    app_installer_data_args_view = *app_installer_data_args;
  }
  Parse(tag, app_installer_data_args_view, args);
}

auto AnyKey() {
  return fuzztest::OneOf(fuzztest::Arbitrary<std::string>(),
                         fuzztest::ElementOf<std::string>({
                             // LINT.IfChange(TagArgs)
                             std::string(kTagArgBundleName),
                             std::string(kTagArgLanguage),
                             std::string(kTagArgFlighting),
                             std::string(kTagArgUsageStats),
                             std::string(kTagArgInstallationId),
                             std::string(kTagArgBrandCode),
                             std::string(kTagArgClientId),
                             std::string(kAppArgExperimentLabels),
                             std::string(kTagArgOmahaExperimentLabels),
                             std::string(kTagArgReferralId),
                             std::string(kAppArgAdditionalParameters),
                             std::string(kTagArgBrowserType),
                             std::string(kTagArgRuntimeMode),
                             std::string(kTagArgEnrollmentToken),
                             std::string(kTagArgAppId),
                             std::string(kAppArgAppName),
                             std::string(kTagArgNeedsAdmin),
                             std::string(kAppArgInstallDataIndex),
                             std::string(kAppArgUntrustedData),
                             std::string(kAppArgInstallerData),
                             // LINT.ThenChange(tag_internal.h:TagArgs)
                         }));
}

auto AnyValue() {
  return fuzztest::OneOf(
      fuzztest::Arbitrary<std::string>(),
      fuzztest::Map([](int i) { return base::NumberToString(i); },
                    fuzztest::Arbitrary<int>()),
      fuzztest::InRegexp(kGUIDRegex),
      fuzztest::ElementOf<std::string>({"true", "false", "persist", "prefers"}),
      fuzztest::InRegexp("(%[0-9a-fA-F]{2})+")  // URL-encoded noise
  );
}

auto TagPair() {
  return fuzztest::Map([](std::string k, std::string v) { return k + "=" + v; },
                       AnyKey(), AnyValue());
}

auto TagString() {
  return fuzztest::Map(
      [](std::vector<std::string> pairs) {
        return base::JoinString(pairs, "&");
      },
      fuzztest::VectorOf(TagPair()));
}

auto SharedParseDomain() {
  return fuzztest::Map(
      [](std::vector<std::string> app_ids,
         std::vector<std::pair<std::string, std::string>> tag_pairs,
         std::vector<std::pair<std::string, std::string>>
             installer_data_pairs) {
        std::vector<std::string> tag_parts;
        for (const auto& p : tag_pairs) {
          tag_parts.push_back(p.first + "=" + p.second);
        }
        for (const auto& id : app_ids) {
          tag_parts.push_back(std::string(kTagArgAppId) + "=" + id);
          tag_parts.push_back(std::string(kAppArgAdditionalParameters) + "=" +
                              id + "_ap");
        }

        std::vector<std::string> id_parts;
        for (const auto& p : installer_data_pairs) {
          id_parts.push_back(p.first + "=" + p.second);
        }
        for (const auto& id : app_ids) {
          id_parts.push_back(std::string(kTagArgAppId) + "=" + id);
          id_parts.push_back(std::string(kAppArgInstallerData) + "=" + id +
                             "_data");
        }

        return SharedInput{base::JoinString(tag_parts, "&"),
                           std::make_optional(base::JoinString(id_parts, "&"))};
      },
      fuzztest::VectorOf(fuzztest::InRegexp(kGUIDRegex)).WithMaxSize(5),
      fuzztest::VectorOf(fuzztest::PairOf(AnyKey(), AnyValue())),
      fuzztest::VectorOf(fuzztest::PairOf(AnyKey(), AnyValue())));
}

void FuzzReadTag(std::vector<uint8_t> data) {
  ReadTag(data);
}

auto BinaryTag() {
  return fuzztest::Map(
      [](std::string prefix, std::vector<std::string> tags,
         std::string suffix) {
        std::vector<uint8_t> result(prefix.begin(), prefix.end());
        for (const auto& tag : tags) {
          result.insert(result.end(), std::begin(kTagMagicUtf8),
                        std::end(kTagMagicUtf8));
          uint16_t len = static_cast<uint16_t>(tag.size());
          result.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
          result.push_back(static_cast<uint8_t>(len & 0xFF));
          result.insert(result.end(), tag.begin(), tag.end());
        }
        result.insert(result.end(), suffix.begin(), suffix.end());
        return result;
      },
      fuzztest::Arbitrary<std::string>(),
      fuzztest::VectorOf(TagString()).WithMaxSize(3),
      fuzztest::Arbitrary<std::string>());
}

// Fuzzes `Parse` with two independent strings. This provides broad coverage
// for general parsing robustness, URL unescaping, and handling of malformed
// key-value pairs that are not necessarily related between the main tag and the
// auxiliary installer data.
void FuzzParseBroad(std::string tag,
                    std::optional<std::string> app_installer_data_args) {
  FuzzParse(tag, app_installer_data_args);
}
FUZZ_TEST(TagFuzzTest, FuzzParseBroad)
    .WithDomains(TagString(), fuzztest::OptionalOf(TagString()));

// Fuzzes `Parse` shared app IDs between the tag and args.
// This specifically targets the "state machine" logic where installerdata
// enrichment depends on the app ID having been previously defined in the
// primary tag.
void FuzzParseShared(SharedInput input) {
  FuzzParse(input.tag, input.app_installer_data_args);
}
FUZZ_TEST(TagFuzzTest, FuzzParseShared).WithDomains(SharedParseDomain());

// Fuzzes `ReadTag` function which extracts tags from binary blobs. This
// exercises the search logic for the magic 'Gact2.0Omaha' signature, big-endian
// length prefix parsing, and out-of-bounds safety when the binary is truncated
// or contains multiple conflicting signatures.
FUZZ_TEST(TagFuzzTest, FuzzReadTag)
    .WithDomains(fuzztest::OneOf(fuzztest::Arbitrary<std::vector<uint8_t>>(),
                                 BinaryTag()));

}  // namespace

}  // namespace updater::tagging
