// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of various reading list preferences.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_PREF_NAMES_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_PREF_NAMES_H_

#include "build/build_config.h"

namespace reading_list {
namespace prefs {

extern const char kReadingListHasUnseenEntries[];

#if !defined(OS_ANDROID) && !defined(OS_IOS)
extern const char kReadingListDesktopFirstUseExperienceShown[];
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

}  // namespace prefs
}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_PREF_NAMES_H_
