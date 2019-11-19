// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/test_hints_component_creator.h"

#include <memory>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string GetDefaultHintVersionString() {
  optimization_guide::proto::Version hint_version;
  hint_version.mutable_generation_timestamp()->set_seconds(123);
  hint_version.set_hint_source(
      optimization_guide::proto::HINT_SOURCE_OPTIMIZATION_HINTS_COMPONENT);
  std::string hint_version_string;
  hint_version.SerializeToString(&hint_version_string);
  base::Base64Encode(hint_version_string, &hint_version_string);
  return hint_version_string;
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
    const std::vector<std::string>& page_hint_host_suffixes,
    const std::string& page_pattern,
    const std::vector<std::string>& resource_blocking_patterns) {
  optimization_guide::proto::Configuration config;
  for (const auto& page_hint_site : page_hint_host_suffixes) {
    optimization_guide::proto::Hint* hint = config.add_hints();
    hint->set_key(page_hint_site);
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    hint->set_version(GetDefaultHintVersionString());

    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern(page_pattern);
    page_hint->set_max_ect_trigger(
        optimization_guide::proto::EFFECTIVE_CONNECTION_TYPE_2G);

    optimization_guide::proto::Optimization* optimization =
        page_hint->add_whitelisted_optimizations();
    optimization->set_optimization_type(optimization_type);
    optimization->mutable_previews_metadata()->set_max_ect_trigger(
        optimization_guide::proto::EFFECTIVE_CONNECTION_TYPE_2G);

    for (auto resource_blocking_pattern : resource_blocking_patterns) {
      optimization_guide::proto::ResourceLoadingHint* resource_loading_hint =
          optimization->mutable_previews_metadata()
              ->add_resource_loading_hints();
      resource_loading_hint->set_loading_optimization_type(
          optimization_guide::proto::LOADING_BLOCK_RESOURCE);
      resource_loading_hint->set_resource_pattern(resource_blocking_pattern);
    }
  }

  // Always stick something with no hint version in here.
  optimization_guide::proto::Hint* no_version_hint = config.add_hints();
  no_version_hint->set_key("noversion.com");
  no_version_hint->set_key_representation(
      optimization_guide::proto::HOST_SUFFIX);
  no_version_hint->add_page_hints()->set_page_pattern("*");
  // Always stick something with a bad hint version in here.
  optimization_guide::proto::Hint* bad_version_hint = config.add_hints();
  bad_version_hint->set_key("badversion.com");
  bad_version_hint->set_key_representation(
      optimization_guide::proto::HOST_SUFFIX);
  bad_version_hint->set_version("notaversion");
  bad_version_hint->add_page_hints()->set_page_pattern("*");

  return WriteConfigToFileAndReturnHintsComponentInfo(config);
}

optimization_guide::HintsComponentInfo
TestHintsComponentCreator::CreateHintsComponentInfoWithExperimentalPageHints(
    optimization_guide::proto::OptimizationType optimization_type,
    const std::vector<std::string>& page_hint_host_suffixes,
    const std::vector<std::string>& experimental_resource_patterns) {
  optimization_guide::proto::Configuration config;
  for (const auto& page_hint_site : page_hint_host_suffixes) {
    optimization_guide::proto::Hint* hint = config.add_hints();
    hint->set_key(page_hint_site);
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    hint->set_version(GetDefaultHintVersionString());

    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("*");
    page_hint->set_max_ect_trigger(
        optimization_guide::proto::EFFECTIVE_CONNECTION_TYPE_2G);

    optimization_guide::proto::Optimization* optimization =
        page_hint->add_whitelisted_optimizations();
    optimization->set_optimization_type(optimization_type);
    optimization->set_experiment_name(kFooExperimentName);
    optimization->mutable_previews_metadata()->set_max_ect_trigger(
        optimization_guide::proto::EFFECTIVE_CONNECTION_TYPE_2G);

    for (auto resource_blocking_pattern : experimental_resource_patterns) {
      optimization_guide::proto::ResourceLoadingHint* resource_loading_hint =
          optimization->mutable_previews_metadata()
              ->add_resource_loading_hints();
      resource_loading_hint->set_loading_optimization_type(
          optimization_guide::proto::LOADING_BLOCK_RESOURCE);
      resource_loading_hint->set_resource_pattern(resource_blocking_pattern);
    }
  }
  // Always stick something with no hint version in here.
  optimization_guide::proto::Hint* no_version_hint = config.add_hints();
  no_version_hint->set_key("noversion.com");
  no_version_hint->set_key_representation(
      optimization_guide::proto::HOST_SUFFIX);
  no_version_hint->add_page_hints()->set_page_pattern("*");
  // Always stick something with a bad hint version in here.
  optimization_guide::proto::Hint* bad_version_hint = config.add_hints();
  bad_version_hint->set_key("badversion.com");
  bad_version_hint->set_key_representation(
      optimization_guide::proto::HOST_SUFFIX);
  bad_version_hint->set_version("notaversion");
  bad_version_hint->add_page_hints()->set_page_pattern("*");

  return WriteConfigToFileAndReturnHintsComponentInfo(config);
}

optimization_guide::HintsComponentInfo
TestHintsComponentCreator::CreateHintsComponentInfoWithMixPageHints(
    optimization_guide::proto::OptimizationType optimization_type,
    const std::vector<std::string>& page_hint_host_suffixes,
    const std::vector<std::string>& experimental_resource_patterns,
    const std::vector<std::string>& default_resource_patterns) {
  optimization_guide::proto::Configuration config;
  for (const auto& page_hint_site : page_hint_host_suffixes) {
    optimization_guide::proto::Hint* hint = config.add_hints();
    hint->set_key(page_hint_site);
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    hint->set_version(GetDefaultHintVersionString());

    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("*");
    page_hint->set_max_ect_trigger(
        optimization_guide::proto::EFFECTIVE_CONNECTION_TYPE_2G);

    // Add experimental patterns first so they get higher priority.
    {
      optimization_guide::proto::Optimization* optimization =
          page_hint->add_whitelisted_optimizations();
      optimization->set_optimization_type(optimization_type);
      optimization->set_experiment_name(kFooExperimentName);
      optimization->mutable_previews_metadata()->set_max_ect_trigger(
          optimization_guide::proto::EFFECTIVE_CONNECTION_TYPE_2G);

      for (auto resource_blocking_pattern : experimental_resource_patterns) {
        optimization_guide::proto::ResourceLoadingHint* resource_loading_hint =
            optimization->mutable_previews_metadata()
                ->add_resource_loading_hints();
        resource_loading_hint->set_loading_optimization_type(
            optimization_guide::proto::LOADING_BLOCK_RESOURCE);
        resource_loading_hint->set_resource_pattern(resource_blocking_pattern);
      }
    }

    {
      optimization_guide::proto::Optimization* optimization =
          page_hint->add_whitelisted_optimizations();
      optimization->set_optimization_type(optimization_type);
      optimization->mutable_previews_metadata()->set_max_ect_trigger(
          optimization_guide::proto::EFFECTIVE_CONNECTION_TYPE_2G);

      for (auto resource_blocking_pattern : default_resource_patterns) {
        optimization_guide::proto::ResourceLoadingHint* resource_loading_hint =
            optimization->mutable_previews_metadata()
                ->add_resource_loading_hints();
        resource_loading_hint->set_loading_optimization_type(
            optimization_guide::proto::LOADING_BLOCK_RESOURCE);
        resource_loading_hint->set_resource_pattern(resource_blocking_pattern);
      }
    }
  }
  // Always stick something with no hint version in here.
  optimization_guide::proto::Hint* no_version_hint = config.add_hints();
  no_version_hint->set_key("noversion.com");
  no_version_hint->set_key_representation(
      optimization_guide::proto::HOST_SUFFIX);
  no_version_hint->add_page_hints()->set_page_pattern("*");
  // Always stick something with a bad hint version in here.
  optimization_guide::proto::Hint* bad_version_hint = config.add_hints();
  bad_version_hint->set_key("badversion.com");
  bad_version_hint->set_key_representation(
      optimization_guide::proto::HOST_SUFFIX);
  bad_version_hint->set_version("notaversion");
  bad_version_hint->add_page_hints()->set_page_pattern("*");

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

  ASSERT_EQ(static_cast<int32_t>(serialized_config.length()),
            base::WriteFile(file_path, serialized_config.data(),
                            serialized_config.length()));
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
