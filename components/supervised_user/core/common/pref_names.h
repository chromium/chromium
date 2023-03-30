// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_PREF_NAMES_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_PREF_NAMES_H_

namespace prefs {

extern const char kSupervisedUserCustodianEmail[];
extern const char kSupervisedUserCustodianName[];
extern const char kSupervisedUserCustodianObfuscatedGaiaId[];
extern const char kSupervisedUserCustodianProfileImageURL[];
extern const char kSupervisedUserCustodianProfileURL[];
extern const char kSupervisedUserSecondCustodianEmail[];
extern const char kSupervisedUserSecondCustodianName[];
extern const char kSupervisedUserSecondCustodianObfuscatedGaiaId[];
extern const char kSupervisedUserSecondCustodianProfileImageURL[];
extern const char kSupervisedUserSecondCustodianProfileURL[];

extern const char kSupervisedUserExtensionsMayRequestPermissions[];
extern const char kSupervisedUserId[];
extern const char kSupervisedUserManualHosts[];
extern const char kSupervisedUserManualURLs[];
extern const char kSupervisedUserMetricsDayId[];
extern const char kSupervisedUserSafeSites[];
extern const char kSupervisedUserSharedSettings[];

extern const char kDefaultSupervisedUserFilteringBehavior[];

}  // namespace prefs

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_PREF_NAMES_H_
