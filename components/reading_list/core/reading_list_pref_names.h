// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for the names of various reading list preferences.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_PREF_NAMES_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_PREF_NAMES_H_

#include "build/build_config.h"

namespace reading_list {
namespace prefs {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
extern const char kReadingListDesktopFirstUseExperienceShown[];
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace prefs
}  // namespace reading_list

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_PREF_NAMES_H_
