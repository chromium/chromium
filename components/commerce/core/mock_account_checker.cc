// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/mock_account_checker.h"

namespace commerce {

MockAccountChecker::MockAccountChecker()
    : AccountChecker(nullptr, nullptr, nullptr, nullptr) {
  // Default to an account checker with the fewest restrictions.
  SetOptedIntoSync(true);
  SetSyncingBookmarks(true);
  SetAnonymizedUrlDataCollectionEnabled(true);
  SetWebAndAppActivityEnabled(true);
  SetIsSubjectToParentalControls(false);
}

MockAccountChecker::~MockAccountChecker() = default;

void MockAccountChecker::SetOptedIntoSync(bool opted_into_sync) {
  ON_CALL(*this, IsOptedIntoSync)
      .WillByDefault(testing::Return(opted_into_sync));
}

void MockAccountChecker::SetSyncingBookmarks(bool syncing) {
  ON_CALL(*this, IsSyncingBookmarks).WillByDefault(testing::Return(syncing));
}

void MockAccountChecker::SetAnonymizedUrlDataCollectionEnabled(bool enabled) {
  ON_CALL(*this, IsAnonymizedUrlDataCollectionEnabled)
      .WillByDefault(testing::Return(enabled));
}

void MockAccountChecker::SetWebAndAppActivityEnabled(bool enabled) {
  ON_CALL(*this, IsWebAndAppActivityEnabled)
      .WillByDefault(testing::Return(enabled));
}

void MockAccountChecker::SetIsSubjectToParentalControls(
    bool subject_to_parental_controls) {
  ON_CALL(*this, IsSubjectToParentalControls)
      .WillByDefault(testing::Return(subject_to_parental_controls));
}

}  // namespace commerce
