// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/json/json_reader.h"
#include "base/test/to_vector.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace web_app {
namespace {
void UpdateManifestCanSuccessfullyParseAnyString(const base::Value& json) {
  auto result = UpdateManifest::CreateFromJson(
      json, GURL("https://example.com/manifest.json"));
}

template <typename T>
std::optional<std::tuple<T>> Wrap(base::optional_ref<const T> maybe_value) {
  return maybe_value.has_value()
             ? std::make_optional(std::tuple<T>{*maybe_value})
             : std::nullopt;
}

auto ArbitraryValueNull() {
  return fuzztest::ReversibleMap(
      []() { return base::Value(); },
      [](const base::Value& value) { return std::optional<std::tuple<>>{}; });
}

auto ArbitraryValueBool() {
  return fuzztest::ReversibleMap(
      [](bool boolean) { return base::Value(boolean); },
      [](const base::Value& value) { return Wrap<bool>(value.GetIfBool()); },
      fuzztest::Arbitrary<bool>());
}

auto ArbitraryValueInt() {
  return fuzztest::ReversibleMap(
      [](int number) { return base::Value(number); },
      [](const base::Value& value) { return Wrap<int>(value.GetIfInt()); },
      fuzztest::Arbitrary<int>());
}

auto ArbitraryValueDouble() {
  return fuzztest::ReversibleMap(
      [](double number) { return base::Value(number); },
      [](const base::Value& value) {
        return Wrap<double>(value.GetIfDouble());
      },
      fuzztest::Finite<double>());
}

auto ArbitraryValueString() {
  return fuzztest::ReversibleMap(
      [](std::string string) { return base::Value(string); },
      [](const base::Value& value) {
        return Wrap<std::string>(value.GetIfString());
      },
      fuzztest::AsciiString());  // TODO(crbug.com/1444407): Should be UTF8
                                 // instead.
}

auto ArbitraryValueBlob() {
  return fuzztest::ReversibleMap(
      [](std::vector<uint8_t> blob) { return base::Value(blob); },
      [](const base::Value& value) {
        return Wrap<std::vector<uint8_t>>(value.GetIfBlob());
      },
      fuzztest::Arbitrary<std::vector<uint8_t>>());
}

auto ArbitraryValueList(fuzztest::Domain<base::Value> entry_domain) {
  return fuzztest::ReversibleMap(
      [](std::vector<base::Value> values) {
        base::Value::List list;
        for (auto& value : values) {
          list.Append(std::move(value));
        }
        return base::Value(std::move(list));
      },
      [](const base::Value& value) {
        auto maybe_list = base::optional_ref(value.GetIfList());
        return maybe_list.has_value()
                   ? std::make_optional(std::tuple{base::test::ToVector(
                         *maybe_list, &base::Value::Clone)})
                   : std::nullopt;
      },
      fuzztest::ContainerOf<std::vector<base::Value>>(entry_domain));
}

auto ArbitraryValueDict(fuzztest::Domain<base::Value> value_domain) {
  return fuzztest::ReversibleMap(
      [](std::vector<std::pair<std::string, base::Value>> entries) {
        base::Value::Dict dict;
        for (auto& [key, value] : entries) {
          dict.Set(std::move(key), std::move(value));
        }
        return base::Value(std::move(dict));
      },
      [](const base::Value& value) {
        auto maybe_dict = base::optional_ref(value.GetIfDict());
        return maybe_dict.has_value()
                   ? std::make_optional(std::tuple{base::test::ToVector(
                         *maybe_dict,
                         [](const auto& entry) {
                           return std::make_pair(entry.first,
                                                 entry.second.Clone());
                         })})
                   : std::nullopt;
      },
      fuzztest::ContainerOf<std::vector<std::pair<std::string, base::Value>>>(
          fuzztest::PairOf(fuzztest::AsciiString(),  // TODO(crbug.com/1444407):
                                                     // Should be UTF8 instead.
                           value_domain)));
}

auto ArbitraryValue() {
  fuzztest::DomainBuilder builder;
  builder.Set<base::Value>(
      "value",
      fuzztest::OneOf(ArbitraryValueNull(), ArbitraryValueBool(),
                      ArbitraryValueInt(), ArbitraryValueDouble(),
                      ArbitraryValueString(), ArbitraryValueBlob(),
                      ArbitraryValueList(builder.Get<base::Value>("value")),
                      ArbitraryValueDict(builder.Get<base::Value>("value"))));
  return std::move(builder).Finalize<base::Value>("value");
}

FUZZ_TEST(UpdateManifestFuzzTest, UpdateManifestCanSuccessfullyParseAnyString)
    .WithDomains(ArbitraryValue())
    .WithSeeds({*base::JSONReader::Read("{}"), *base::JSONReader::Read(R"({
                  "versions": []
                })"),
                *base::JSONReader::Read(R"({
                  "versions": [
                    {
                      "version": "1.0.0",
                      "url": "https://example.com/bundle.swbn"
                    },
                    {
                      "version": "1.0.3",
                      "url": "bundle.swbn"
                    },
                    {
                      "version": "1.0.0",
                      "url": "https://example.com/bundle2.swbn"
                    }
                  ]
                })"),
                *base::JSONReader::Read(R"({
                  "versions": [
                    {
                      "version": "1.0.0",
                      "url": "https://example.com/bundle.swbn",
                      "blah": 123
                    }
                  ]
                })")});
}  // namespace
}  // namespace web_app
