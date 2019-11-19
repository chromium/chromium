// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_PUP_DATA_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_PUP_DATA_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/chrome_cleaner/proto/shared_pup_enums.pb.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/pup_data/uws_catalog.h"

namespace chrome_cleaner {

// A PUP with fixed sample values in its signature.
class SimpleTestPUP : public PUPData::PUP {
 public:
  SimpleTestPUP();
};

// This class implements an UwSCatalog that can have UwS added and removed from
// it dynamically so that each test can generate its own test values. (Compare
// TestUwSCatalog which contains a standardized set of test UwS.) It can also
// accept an additional set of UwS catalogs that are registered with PUPData at
// the same time.
//
// It also adds test utility methods to generate raw data that used to be used
// by PUP Data.
// TODO(joenotcharles): Remove all the complexity of cpp_pup_footprints_ and
// mirror_pup_footprints_.
class TestPUPData : public UwSCatalog {
 public:
  typedef std::vector<PUPData::StaticDiskFootprint> RawDiskFootprints;
  typedef std::vector<PUPData::StaticRegistryFootprint> RawRegistryFootprints;
  typedef std::vector<PUPData::CustomMatcher> RawCustomMatchers;

  TestPUPData();
  ~TestPUPData() override;

  // Empties all the C++ arrays, clears the main C array, and registers this
  // UwSCatalog and |additional_uws_catalogs| with PUPData.
  void Reset(const PUPData::UwSCatalogs& additional_uws_catalogs);

  // Add entries to the different arrays, and set the C array to the data
  // pointers of their respective C++ arrays.
  void AddPUP(UwSId pup_id,
              PUPData::Flags flags,
              const char* name,
              size_t max_files_to_remove);
  void AddDiskFootprint(UwSId pup_id,
                        int csidl,
                        const wchar_t* path,
                        PUPData::DiskMatchRule rule);
  void AddRegistryFootprint(UwSId pup_id,
                            RegistryRoot registry_root,
                            const wchar_t* key_path,
                            const wchar_t* value_name,
                            const wchar_t* value_substring,
                            RegistryMatchRule rule);
  void AddCustomMatcher(UwSId pup_id, PUPData::CustomMatcher matcher);

  // This structure hold on C++ strings/arrays for a given PUP. This is to allow
  // a proper memory management for the C arrays in the raw PUP structure.
  struct CPPPUP {
    std::string name;
    RawDiskFootprints disk_footprints;
    RawRegistryFootprints registry_footprints;
    RawCustomMatchers custom_matchers;

    CPPPUP();
    ~CPPPUP();
  };
  typedef std::map<UwSId, CPPPUP> CPPPUPMap;

  // This is used by the PUPData tests to compare the C++ data generation
  // with the tests data.
  const CPPPUPMap& cpp_pup_footprints() const { return cpp_pup_footprints_; }

  // Return the map of all PUPs cached in PUPData.
  const PUPData::PUPDataMap* GetAllPUPs() const {
    return PUPData::GetAllPUPs();
  }

  // UwSCatalog

  std::vector<UwSId> GetUwSIds() const override;

  bool IsEnabledForScanning(UwSId id) const override;

  bool IsEnabledForCleaning(UwSId id) const override;

  std::unique_ptr<PUPData::PUP> CreatePUPForId(UwSId id) const override;

 private:
  // This makes sure an entry for this PUP exists in the CPP and mirror arrays
  // and return the index of the existing or newly created entry in the mirror.
  size_t EnsurePUP(UwSId pup_id);

  // A map of all the PUP C++ data.
  CPPPUPMap cpp_pup_footprints_;

  // A C++ vector for the raw PUPs C array of pointer, which will then be
  // pointing to the C++ data held in |cpp_pup_footprints_|, thus, a mirror.
  std::vector<PUPData::UwSSignature> mirror_pup_footprints_;

  // UwSCatalogs in use when the test starts, to be reset afterwards.
  const PUPData::UwSCatalogs previous_catalogs_;

  // This is to ensure that there are no other tests of this type ran in
  // parallel because we rely on static data not changing.
  static TestPUPData* current_test_;
  // But we support embedding, as long as no methods are called interleaved.
  static TestPUPData* previous_test_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_PUP_DATA_H_
