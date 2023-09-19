// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/crosier_util.h"

#include "base/test/gtest_tags.h"

namespace crosier_util {

void AddTestInfo(const chrome_test_base_chromeos_crosier::TestInfo& info) {
  for (int i = 0; i < info.contacts_size(); ++i) {
    // This field name aligns with existing Tast test format.
    base::AddTagToTestResult("contacts", info.contacts(i));
  }
  if (info.has_team_email()) {
    // This field name aligns with 'team_email' in DIR_METADATA.
    base::AddTagToTestResult("team_email", info.team_email());
  }
  if (info.has_buganizer()) {
    // This field name aligns with 'buganizer' in DIR_METADATA.
    base::AddTagToTestResult("buganizer", info.buganizer());
  }
  if (info.has_buganizer_public()) {
    // This field name aligns with 'buganizer_public' in DIR_METADATA.
    base::AddTagToTestResult("buganizer_public", info.buganizer_public());
  }
}

}  // namespace crosier_util
