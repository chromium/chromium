// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Author: Ken Chen <kenchen@google.com>
//
// Library support to remap process executable elf segment with hugepages.
//
// InitHugepagesAndMlockSelf() will search for an ELF executable segment,
// and remap it using hugepage.

#ifndef CHROMEOS_HUGEPAGE_TEXT_HUGEPAGE_TEXT_H_
#define CHROMEOS_HUGEPAGE_TEXT_HUGEPAGE_TEXT_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/chromeos_export.h"

#if defined(__clang__) || defined(__GNUC__)
#define ATTRIBUTE_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))
#else
#define ATTRIBUTE_NO_SANITIZE_ADDRESS
#endif

namespace chromeos {

// A feature which controls remapping the zygotes hot text section as hugepages
// and locking.
extern const base::Feature kCrOSHugepageRemapAndLockZygote;

// This function will scan ELF segments and attempt to do two things:
// - Reload some of .text into hugepages
// - Lock some of .text into memory, so it can't be swapped out.
//
// When this function returns, text segments that are naturally aligned on
// hugepage size will be backed by hugepages.
//
// Any and all errors encountered by this function are soft errors; Chrome
// should still be able to run.
CHROMEOS_EXPORT extern void InitHugepagesAndMlockSelf(void);
}  // namespace chromeos

#endif  // CHROMEOS_HUGEPAGE_TEXT_HUGEPAGE_TEXT_H_
