// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONSTANTS_H_
#define CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONSTANTS_H_

namespace chromeos::magic_boost {

// The view ids for Magic Boost related views.
enum ViewId {
  OptInCardSecondaryButton = 1,
  OptInCardPrimaryButton,
  OptInCardTitleLabel,
  OptInCardBodyLabel,
};

// The features that trigger the magic boost opt in card showing. `kTotal` means
// no filter is added, which is used to record the total number.
enum class OptInFeatures {
  kHmrOnly = 0,
  kOrcaAndHmr = 1,
  kOrcaOnly = 2,
  kTotal = 3,
  kMaxValue = kTotal,
};

}  // namespace chromeos::magic_boost

#endif  // CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONSTANTS_H_
