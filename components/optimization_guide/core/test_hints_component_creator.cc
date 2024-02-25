// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/test_hints_component_creator.h"

#include <memory>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "components/optimization_guide/core/bloom_filter.h"
#include "components/optimization_guide/core/optimization_hints_component_update_listener.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string GetDefaultHintVersionString() {
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(123);
  hint_version.set_hint_source(
      optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_HINTS_COMPONENT);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  return base::Base64Encode(hint_version_string);
}

}  // namespace

namespace optimization_guide {
namespace testing {

TestHintsComponentCreator::TestHintsComponentCreator()
    : scoped_temp_dir_(std::make_unique<base::ScopedTempDir>()),
      next_component_version_(1) {}

TestHintsComponentCreator::~TestHintsComponentCreator() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  scoped_temp_dir_.reset();
}

optimization_guide::HintsComponentInfo
TestHintsComponentCreator::CreateHintsComponentInfoWithPageHints(
    optimization_guide::proto::OptimizationType optimization_type,
    const std::vector<std::string>& page_hint_hosts,
    const std::string& page_pattern) {
  optimization_guide::proto::Configuration config;
  for (const auto& page_hint_site : page_hint_hosts) {
    optimization_guide::proto::Hint* hint = config.add_hints();
    hint->set_key(page_hint_site);
    hint->set_key_representation(optimization_guide::proto::HOST);
    hint->set_version(GetDefaultHintVersionString());

    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern(page_pattern);

    optimization_guide::proto::Optimization* optimization =
        page_hint->add_allowlisted_optimizations();
    optimization->set_optimization_type(optimization_type);
  }

  // Always stick something with no hint version in here.
  optimization_guide::proto::Hint* no_version_hint = config.add_hints();
  no_version_hint->set_key("noversion.com");
  no_version_hint->set_key_representation(optimization_guide::proto::HOST);
  no_version_hint->add_page_hints()->set_page_pattern("*");
  // Always stick something with a bad hint version in here.
  optimization_guide::proto::Hint* bad_version_hint = config.add_hints();
  bad_version_hint->set_key("badversion.com");
  bad_version_hint->set_key_representation(optimization_guide::proto::HOST);
  bad_version_hint->set_version("notaversion");
  bad_version_hint->add_page_hints()->set_page_pattern("*");

  // Always stick an allowlist optimization filter in here.
  optimization_guide::BloomFilter allowlist_bloom_filter(7, 511);
  allowlist_bloom_filter.Add("allowedhost.com");
  std::string allowlist_bloom_filter_data(
      reinterpret_cast<const char*>(&allowlist_bloom_filter.bytes()[0]),
      allowlist_bloom_filter.bytes().size());
  optimization_guide::proto::OptimizationFilter* allowlist_optimization_filter =
      config.add_optimization_allowlists();
  allowlist_optimization_filter->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  allowlist_optimization_filter->mutable_bloom_filter()->set_num_hash_functions(
      7);
  allowlist_optimization_filter->mutable_bloom_filter()->set_num_bits(511);
  allowlist_optimization_filter->mutable_bloom_filter()->set_data(
      allowlist_bloom_filter_data);
  // Always stick a blocklist optimization filter in here.
  optimization_guide::BloomFilter blocklist_bloom_filter(7, 511);
  blocklist_bloom_filter.Add("blockedhost.com");
  std::string blocklist_bloom_filter_data(
      reinterpret_cast<const char*>(&blocklist_bloom_filter.bytes()[0]),
      blocklist_bloom_filter.bytes().size());
  optimization_guide::proto::OptimizationFilter* blocklist_optimization_filter =
      config.add_optimization_blocklists();
  blocklist_optimization_filter->set_optimization_type(
      optimization_guide::proto::FAST_HOST_HINTS);
  blocklist_optimization_filter->mutable_bloom_filter()->set_num_hash_functions(
      7);
  blocklist_optimization_filter->mutable_bloom_filter()->set_num_bits(511);
  blocklist_optimization_filter->mutable_bloom_filter()->set_data(
      blocklist_bloom_filter_data);

  return WriteConfigToFileAndReturnHintsComponentInfo(config);
}

base::FilePath TestHintsComponentCreator::GetFilePath(
    std::string file_path_suffix) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(scoped_temp_dir_->IsValid() ||
              scoped_temp_dir_->CreateUniqueTempDir());
  return scoped_temp_dir_->GetPath().AppendASCII(file_path_suffix);
}

void TestHintsComponentCreator::WriteConfigToFile(
    const base::FilePath& file_path,
    const optimization_guide::proto::Configuration& config) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string serialized_config;
  ASSERT_TRUE(config.SerializeToString(&serialized_config));

  ASSERT_TRUE(base::WriteFile(file_path, serialized_config));
}

optimization_guide::HintsComponentInfo
TestHintsComponentCreator::WriteConfigToFileAndReturnHintsComponentInfo(
    const optimization_guide::proto::Configuration& config) {
  std::string version_string = base::NumberToString(next_component_version_++);
  base::FilePath file_path = GetFilePath(version_string);
  WriteConfigToFile(file_path, config);
  return optimization_guide::HintsComponentInfo(base::Version(version_string),
                                                file_path);
}

}  // namespace testing
}  // namespace optimization_guide
