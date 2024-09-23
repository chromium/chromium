// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/memory/elf_sections.h"

#include <cstdint>

namespace ash {

// The location of ELF sections are determined after compile. These values are
// defined as 0 as a placeholder and will be filled with actual values by
// `build/chromeos/embed_sections.py`.
// :section_embedded_chrome_binary target generates `chrome.sections_embedded`
// using embed_sections.py.
//
// __attribute__((used)) + const makes these global variables be in .rodata
// section. volatile indicates that these variables may change in ways that
// can't be modelled by the compiler, so prevents propagating these values.
//
// These variables need to be defined as a separated file while
// `chromeos/ash/components/memory/memory.cc` is the only customer. Otherwise
// these variables are not placed in .rodata section.
#define CROS_POSTLINK_INITED_CONST __attribute__((used)) const volatile

CROS_POSTLINK_INITED_CONST uintptr_t kRodataAddr = 0;
CROS_POSTLINK_INITED_CONST uint64_t kRodataSize = 0;
CROS_POSTLINK_INITED_CONST uintptr_t kTextHotAddr = 0;
CROS_POSTLINK_INITED_CONST uint64_t kTextHotSize = 0;

#undef CROS_POSTLINK_INITED_CONST
}  // namespace ash
