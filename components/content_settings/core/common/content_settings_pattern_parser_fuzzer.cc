// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_pattern_parser.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>

#include "components/content_settings/core/common/content_settings_pattern.h"
#include "third_party/icu/fuzzers/fuzzer_utils.h"

IcuEnvironment* env = new IcuEnvironment();

namespace content_settings {

namespace {
ContentSettingsPattern Parse(std::string_view pattern_spec) {
  std::unique_ptr<ContentSettingsPattern::BuilderInterface> builder =
      ContentSettingsPattern::CreateBuilder();
  PatternParser::Parse(pattern_spec, builder.get());
  return builder->Build();
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view pattern_spec(reinterpret_cast<const char*>(data), size);

  // Parse the fuzzer-generated |pattern_spec| to obtain |canonical_pattern|.
  ContentSettingsPattern canonical_pattern = Parse(pattern_spec);
  if (!canonical_pattern.IsValid())
    return 0;
  const std::string canonical_pattern_spec = canonical_pattern.ToString();

  // Recanonicalizing |canonical_pattern| should be idempotent.
  ContentSettingsPattern recanonicalized_pattern =
      Parse(canonical_pattern_spec);
  CHECK(recanonicalized_pattern.IsValid())
      << "Could not recanonicalize\n  '" << canonical_pattern_spec
      << "' (originally '" << pattern_spec << "')";
  CHECK_EQ(recanonicalized_pattern.ToString(), canonical_pattern_spec)
      << "\n  (originally '" << pattern_spec << "')";
  CHECK_EQ(recanonicalized_pattern.Compare(canonical_pattern),
           ContentSettingsPattern::Relation::IDENTITY)
      << "Canonical pattern\n"
      << canonical_pattern.ToString() << "\nand recanonicalized pattern\n"
      << recanonicalized_pattern.ToString() << "\nwere not identical";
  return 0;
}

}  // namespace content_settings
