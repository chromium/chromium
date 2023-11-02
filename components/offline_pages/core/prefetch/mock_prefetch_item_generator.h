// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_MOCK_PREFETCH_ITEM_GENERATOR_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_MOCK_PREFETCH_ITEM_GENERATOR_H_

#include <string>

#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"

namespace offline_pages {

// Generator of PrefetchItem instances with all fields automatically
// pre-populated with values that are reasonable and unique (within the
// instance). To further customize returned items one can set custom prefixes or
// just change the actual values of returned instances.  When creating an item
// with a particular state, only the fields applicable to that state will be
// populated, and the rest will remain in their default state, even if prefixes
// are set for that data member.
class MockPrefetchItemGenerator {
 public:
  static const std::string kClientNamespace;
  static const std::string kClientIdPrefix;
  static const std::string kUrlPrefix;
  static const std::string kFinalUrlPrefix;
  static const std::string kOperationNamePrefix;
  static const std::string kArchiveBodyNamePrefix;
  static const std::string kTitlePrefix;
  static const std::string kFilePathPrefix;

  MockPrefetchItemGenerator();
  ~MockPrefetchItemGenerator();

  // Creates a new item using all set prefixes and an internal counter to set
  // reasonable and unique values to all fields including the instance-unique
  // offline ID.
  PrefetchItem CreateItem(PrefetchItemState state);

  // Generates a unique offline ID within the context of this generator
  // instance. Values will not be unique among different instances.
  int64_t GenerateTestOfflineId();

  // Setters for all prefixes.
  void set_client_namespace(std::string client_namespace) {
    client_namespace_ = client_namespace;
  }
  void set_client_id_prefix(std::string client_id_prefix) {
    client_id_prefix_ = client_id_prefix;
  }
  void set_url_prefix(std::string url_prefix) { url_prefix_ = url_prefix; }
  void set_final_url_prefix(std::string final_url_prefix) {
    final_url_prefix_ = final_url_prefix;
  }
  void set_operation_name_prefix(std::string operation_name_prefix) {
    operation_name_prefix_ = operation_name_prefix;
  }
  void set_archive_body_name_prefix(std::string archive_body_name_prefix) {
    archive_body_name_prefix_ = archive_body_name_prefix;
  }
  void set_title_prefix(std::string title_prefix) {
    title_prefix_ = title_prefix;
  }
  void set_file_path_prefix(std::string file_path_prefix) {
    file_path_prefix_ = file_path_prefix;
  }

 private:
  // These namespace name and prefixes must always be set.
  std::string client_namespace_ = kClientNamespace;
  std::string client_id_prefix_ = kClientIdPrefix;
  std::string url_prefix_ = kUrlPrefix;

  // These prefixes, if custom set to the empty string, will cause related
  // values not to be set.
  std::string final_url_prefix_ = kFinalUrlPrefix;
  std::string operation_name_prefix_ = kOperationNamePrefix;
  std::string archive_body_name_prefix_ = kArchiveBodyNamePrefix;
  std::string title_prefix_ = kTitlePrefix;
  std::string file_path_prefix_ = kFilePathPrefix;

  // Test offline IDs start at an arbitrary, non-zero value to ease recognizing
  // generated ID values among other integer values while debugging.
  int64_t offline_id_counter_ = 1000;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_MOCK_PREFETCH_ITEM_GENERATOR_H_
