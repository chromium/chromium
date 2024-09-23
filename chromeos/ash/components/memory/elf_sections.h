// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_ELF_SECTIONS_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_ELF_SECTIONS_H_

#include <cstdint>

namespace ash {
// The beginning address of .rodata ELF section in the chrome binary.
extern "C" const volatile uintptr_t kRodataAddr;
// The size of .rodata ELF section in the chrome binary.
extern "C" const volatile uint64_t kRodataSize;
// The beginning address of .text.hot ELF section in the chrome binary.
extern "C" const volatile uintptr_t kTextHotAddr;
// The size of .text.hot ELF section in the chrome binary.
extern "C" const volatile uint64_t kTextHotSize;
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_ELF_SECTIONS_H_
