// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_pup_data.h"

#include "base/logging.h"
#include "chrome/chrome_cleaner/proto/shared_pup_enums.pb.h"
#include "chrome/chrome_cleaner/pup_data/uws_catalog.h"

namespace chrome_cleaner {

namespace {

const PUPData::UwSSignature kEmptyPUP = {PUPData::kInvalidUwSId,
                                         PUPData::FLAGS_NONE,
                                         nullptr,
                                         0,
                                         kNoDisk,
                                         kNoRegistry,
                                         kNoCustomMatcher};

}  // namespace

TestPUPData* TestPUPData::current_test_ = nullptr;
TestPUPData* TestPUPData::previous_test_ = nullptr;

SimpleTestPUP::SimpleTestPUP() : PUPData::PUP(&kEmptyPUP) {}

TestPUPData::TestPUPData() : previous_catalogs_(PUPData::GetUwSCatalogs()) {
  // Make sure tests don't step on each other.
  previous_test_ = current_test_;
  current_test_ = this;

  // Assume there will be no additional UwSCatalogs used during the test. If
  // not, the caller can call Reset again with an explicit list of catalogs.
  Reset({});
}

TestPUPData::~TestPUPData() {
  // Reset the global PUP data to the non-test version.
  PUPData::InitializePUPData(previous_catalogs_);

  CHECK_EQ(this, current_test_);
  current_test_ = previous_test_;
}

void TestPUPData::Reset(const PUPData::UwSCatalogs& additional_uws_catalogs) {
  CHECK_EQ(this, current_test_);
  cpp_pup_footprints_.clear();
  mirror_pup_footprints_.clear();

  PUPData::UwSCatalogs uws_catalogs = additional_uws_catalogs;
  uws_catalogs.push_back(this);
  PUPData::InitializePUPData(uws_catalogs);
}

void TestPUPData::AddPUP(UwSId pup_id,
                         PUPData::Flags flags,
                         const char* name,
                         size_t max_files_to_remove) {
  CHECK_EQ(this, current_test_);
  size_t mirror_index = EnsurePUP(pup_id);
  mirror_pup_footprints_[mirror_index].flags = flags;
  mirror_pup_footprints_[mirror_index].max_files_to_remove =
      max_files_to_remove;
  // We keep a C++ version of the strings to control lifespan of the pointer
  // put in the C array.
  if (name) {
    cpp_pup_footprints_[pup_id].name = name;
    mirror_pup_footprints_[mirror_index].name =
        cpp_pup_footprints_[pup_id].name.c_str();
  } else {
    cpp_pup_footprints_[pup_id].name.clear();
    mirror_pup_footprints_[mirror_index].name = nullptr;
  }
}

void TestPUPData::AddDiskFootprint(UwSId pup_id,
                                   int csidl,
                                   const wchar_t* path,
                                   PUPData::DiskMatchRule rule) {
  CHECK_EQ(this, current_test_);
  size_t mirror_index = EnsurePUP(pup_id);

  PUPData::StaticDiskFootprint footprint;
  footprint.csidl = csidl;
  footprint.path = path;
  footprint.rule = rule;

  // Insert before the null terminating entry.
  cpp_pup_footprints_[pup_id].disk_footprints.insert(
      cpp_pup_footprints_[pup_id].disk_footprints.end() - 1, footprint);
  mirror_pup_footprints_[mirror_index].disk_footprints =
      cpp_pup_footprints_[pup_id].disk_footprints.data();
}

void TestPUPData::AddRegistryFootprint(UwSId pup_id,
                                       RegistryRoot registry_root,
                                       const wchar_t* key_path,
                                       const wchar_t* value_name,
                                       const wchar_t* value_substring,
                                       RegistryMatchRule rule) {
  CHECK_EQ(this, current_test_);
  size_t mirror_index = EnsurePUP(pup_id);

  // Sanity checks to avoid making silly mistakes in unittests.
  CHECK(key_path);
  if (rule == REGISTRY_VALUE_MATCH_KEY) {
    CHECK(!value_name);
    CHECK(!value_substring);
  } else if (rule == REGISTRY_VALUE_MATCH_VALUE_NAME) {
    CHECK(value_name);
    CHECK(!value_substring);
  } else {
    CHECK(value_name);
    CHECK(value_substring);
  }

  PUPData::StaticRegistryFootprint footprint;
  footprint.registry_root = registry_root;
  footprint.key_path = key_path;
  footprint.value_name = value_name;
  footprint.value_substring = value_substring;
  footprint.rule = rule;

  // Insert before the null terminating entry.
  cpp_pup_footprints_[pup_id].registry_footprints.insert(
      cpp_pup_footprints_[pup_id].registry_footprints.end() - 1, footprint);
  mirror_pup_footprints_[mirror_index].registry_footprints =
      cpp_pup_footprints_[pup_id].registry_footprints.data();
}

TestPUPData::CPPPUP::CPPPUP() = default;

TestPUPData::CPPPUP::~CPPPUP() = default;

void TestPUPData::AddCustomMatcher(UwSId pup_id,
                                   PUPData::CustomMatcher matcher) {
  CHECK_EQ(this, current_test_);
  size_t mirror_index = EnsurePUP(pup_id);

  // Insert before the null terminating entry.
  cpp_pup_footprints_[pup_id].custom_matchers.insert(
      cpp_pup_footprints_[pup_id].custom_matchers.end() - 1, matcher);
  mirror_pup_footprints_[mirror_index].custom_matchers =
      cpp_pup_footprints_[pup_id].custom_matchers.data();
}

size_t TestPUPData::EnsurePUP(UwSId pup_id) {
  if (mirror_pup_footprints_.empty()) {
    // If we are adding the first entry, we need an initial one for the null
    // terminating entry.
    mirror_pup_footprints_.push_back(kEmptyPUP);
  } else {
    for (size_t i = 0; i < mirror_pup_footprints_.size(); ++i) {
      if (mirror_pup_footprints_[i].id == pup_id)
        return i;
    }
  }

  // Make sure the last entry is a null terminating entry.
  size_t index = mirror_pup_footprints_.size() - 1;
  DCHECK_EQ(PUPData::kInvalidUwSId, mirror_pup_footprints_[index].id);

  // Set up the new value to be inserted before the null terminating entry.
  PUPData::UwSSignature new_pup;
  new_pup.id = pup_id;
  new_pup.flags = PUPData::FLAGS_ACTION_REMOVE;

  // Initialize all C++ arrays with a null terminating entry, and use their data
  // pointers in the C array mirror.
  cpp_pup_footprints_[pup_id].disk_footprints.push_back({});
  new_pup.disk_footprints = cpp_pup_footprints_[pup_id].disk_footprints.data();
  cpp_pup_footprints_[pup_id].registry_footprints.push_back({});
  new_pup.registry_footprints =
      cpp_pup_footprints_[pup_id].registry_footprints.data();
  cpp_pup_footprints_[pup_id].custom_matchers.push_back({});
  new_pup.custom_matchers = cpp_pup_footprints_[pup_id].custom_matchers.data();

  mirror_pup_footprints_.insert(mirror_pup_footprints_.end() - 1, new_pup);

  // Add any newly-created UwS to the cache. Don't touch anything that already
  // existed, since this is often called during tests that have pointers to
  // that UwS.
  PUPData::UpdateCachedUwSForTesting();

  return index;
}

std::vector<UwSId> TestPUPData::GetUwSIds() const {
  std::vector<UwSId> ids;
  ids.reserve(mirror_pup_footprints_.size());
  for (const PUPData::UwSSignature& signature : mirror_pup_footprints_) {
    if (signature.id != PUPData::kInvalidUwSId)
      ids.push_back(signature.id);
  }
  return ids;
}

bool TestPUPData::IsEnabledForScanning(UwSId id) const {
  DCHECK_NE(id, PUPData::kInvalidUwSId);
  for (const PUPData::UwSSignature& signature : mirror_pup_footprints_) {
    if (signature.id == id)
      return true;
  }
  return false;
}

bool TestPUPData::IsEnabledForCleaning(UwSId id) const {
  DCHECK_NE(id, PUPData::kInvalidUwSId);
  for (const PUPData::UwSSignature& signature : mirror_pup_footprints_) {
    if (signature.id == id && signature.flags & PUPData::FLAGS_ACTION_REMOVE)
      return true;
  }
  return false;
}

std::unique_ptr<PUPData::PUP> TestPUPData::CreatePUPForId(UwSId id) const {
  DCHECK_NE(id, PUPData::kInvalidUwSId);
  for (const PUPData::UwSSignature& signature : mirror_pup_footprints_) {
    if (signature.id == id)
      return std::make_unique<PUPData::PUP>(&signature);
  }
  NOTREACHED() << id << " not in TestPUPData";
  return nullptr;
}

}  // namespace chrome_cleaner
