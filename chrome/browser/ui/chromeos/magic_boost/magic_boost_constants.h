// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONSTANTS_H_
#define CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONSTANTS_H_

#include "build/branding_buildflags.h"
#include "chromeos/strings/grit/chromeos_strings.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/app/theme/google_chrome/chromeos/strings/grit/chromeos_chrome_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace chromeos::magic_boost {

// The view ids for Magic Boost related views.
enum ViewId {
  OptInCardSecondaryButton = 1,
  OptInCardPrimaryButton,
};

// TODO(b/331127382): Finalize the Mahi menu title.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
inline constexpr int kMahiMenuTitleStringId = IDS_MAHI_MENU_TITLE;
#else
inline constexpr int kMahiMenuTitleStringId = IDS_MAHI_MENU_TITLE_SHORT;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace chromeos::magic_boost

#endif  // CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONSTANTS_H_
