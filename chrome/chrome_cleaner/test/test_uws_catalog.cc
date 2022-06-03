// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_uws_catalog.h"

#include "base/notreached.h"
#include "chrome/chrome_cleaner/pup_data/dynamic_pup.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"

namespace chrome_cleaner {

// static
const TestUwSCatalog& TestUwSCatalog::GetInstance() {
  static TestUwSCatalog* unique_instance = new TestUwSCatalog();
  return *unique_instance;
}

std::vector<UwSId> TestUwSCatalog::GetUwSIds() const {
  return {kGoogleTestAUwSID, kGoogleTestBUwSID};
}

bool TestUwSCatalog::IsEnabledForScanning(UwSId id) const {
  return id == kGoogleTestAUwSID || id == kGoogleTestBUwSID;
}

bool TestUwSCatalog::IsEnabledForCleaning(UwSId id) const {
  // kGoogleTestAUwSID is scan-only.
  return id == kGoogleTestBUwSID;
}

std::unique_ptr<PUPData::PUP> TestUwSCatalog::CreatePUPForId(
    chrome_cleaner::UwSId id) const {
  std::string name;
  if (id == kGoogleTestAUwSID) {
    name = "GoogleTestA";
  } else if (id == kGoogleTestBUwSID) {
    name = "GoogleTestB";
  } else {
    // Unrecognized UwS.
    NOTREACHED();
    return nullptr;
  }
  PUPData::Flags flags = PUPData::FLAGS_NONE;
  if (IsEnabledForCleaning(id)) {
    // All removable UwS must also be flagged as confirmed malicious.
    flags |= PUPData::FLAGS_STATE_CONFIRMED_UWS | PUPData::FLAGS_ACTION_REMOVE;
  }

  return std::make_unique<DynamicPUP>(name, id, flags);
}

TestUwSCatalog::TestUwSCatalog() = default;

TestUwSCatalog::~TestUwSCatalog() = default;

}  // namespace chrome_cleaner
