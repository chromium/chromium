// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_PROFILE_ERROR_DIALOG_H_
#define CHROME_BROWSER_UI_PROFILES_PROFILE_ERROR_DIALOG_H_

#include <string>

// Be very careful while modifying this enum. Do NOT remove any elements from
// this enum. If you need to add one, add them to the end, right before END.
// END should ALWAYS be the last element in this enum. This is important because
// this enum is used to back a histogram, and these are implicit assumptions
// made in terms of how enumerated histograms are defined.
enum class ProfileErrorType {
  HISTORY = 0,
  PREFERENCES = 1,
  DB_AUTOFILL_WEB_DATA = 2,
  DB_TOKEN_WEB_DATA = 3,
  DB_WEB_DATA = 4,
  DB_KEYWORD_WEB_DATA = 5,
  CREATE_FAILURE_SPECIFIED = 6,
  CREATE_FAILURE_ALL = 7,
  DB_PAYMENT_MANIFEST_WEB_DATA = 8,
  DB_ACCOUNT_AUTOFILL_WEB_DATA = 9,
  kMaxValue = DB_ACCOUNT_AUTOFILL_WEB_DATA,
};

// Shows an error dialog corresponding to the inability to open some portion of
// the profile.
// The ProfileErrorType |type| needs to correspond to one of the profile error
// types in the enum above. If your use case doesn't fit any of the ones listed
// above, please add your type to the enum and modify the enum definition in
// tools/metrics/histograms/histograms.xml accordingly.
// |message_id| is a string id corresponding to the message to show.
// |diagnostics| contains diagnostic information about the database file that
// might have caused a profile error.
void ShowProfileErrorDialog(ProfileErrorType type,
                            int message_id,
                            const std::string& diagnostics);

#endif  // CHROME_BROWSER_UI_PROFILES_PROFILE_ERROR_DIALOG_H_
