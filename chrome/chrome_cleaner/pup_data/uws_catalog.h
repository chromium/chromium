// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PUP_DATA_UWS_CATALOG_H_
#define CHROME_CHROME_CLEANER_PUP_DATA_UWS_CATALOG_H_

#include <memory>
#include <vector>

#include "chrome/chrome_cleaner/constants/uws_id.h"

namespace chrome_cleaner {

class UwSCatalog {
 public:
  virtual ~UwSCatalog() = default;

  // Returns the id's of all UwS in the catalog.
  virtual std::vector<UwSId> GetUwSIds() const = 0;

  // Returns whether the UwS represented by |id| should be scanned.
  virtual bool IsEnabledForScanning(UwSId id) const = 0;

  // Returns whether the UwS represented by |id| should be cleaned.
  virtual bool IsEnabledForCleaning(UwSId id) const = 0;

  // Returns a newly created PUP structure for the UwS represented by |id|.
  virtual std::unique_ptr<PUPData::PUP> CreatePUPForId(UwSId id) const = 0;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PUP_DATA_UWS_CATALOG_H_
