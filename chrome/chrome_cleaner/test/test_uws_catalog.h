// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_UWS_CATALOG_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_UWS_CATALOG_H_

#include <memory>
#include <vector>

#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/pup_data/uws_catalog.h"

namespace chrome_cleaner {

class TestUwSCatalog : public UwSCatalog {
 public:
  // Returns the global catalog instance.
  static const TestUwSCatalog& GetInstance();

  // UwSCatalog

  // Returns a the id's of all UwS in the catalog.
  std::vector<UwSId> GetUwSIds() const override;

  // Returns whether the UwS represented by |id| should be scanned.
  bool IsEnabledForScanning(UwSId id) const override;

  // Returns whether the UwS represented by |id| should be cleaned.
  bool IsEnabledForCleaning(UwSId id) const override;

  // Returns a newly created PUP structure for the UwS represented by |id|.
  std::unique_ptr<PUPData::PUP> CreatePUPForId(UwSId id) const override;

 private:
  TestUwSCatalog();
  ~TestUwSCatalog() override;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_UWS_CATALOG_H_
