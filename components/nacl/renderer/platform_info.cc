// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cpu.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace nacl {

const char* GetSandboxArch() {
#if defined(ARCH_CPU_ARM_FAMILY)
  return "arm";
#elif defined(ARCH_CPU_MIPS_FAMILY)
  return "mips32";
#elif defined(ARCH_CPU_X86_FAMILY)

#if defined(OS_WIN)
  // We have to check the host architecture on Windows.
  // See sandbox_isa.h for an explanation why.
  if (base::win::OSInfo::GetArchitecture() ==
      base::win::OSInfo::X64_ARCHITECTURE) {
    return "x86-64";
  }
  return "x86-32";
#elif ARCH_CPU_64_BITS
  return "x86-64";
#else
  return "x86-32";
#endif  // defined(OS_WIN)

#else
#error Architecture not supported.
#endif
}

std::string GetCpuFeatures() {
  // PNaCl's translator from pexe to nexe can be told exactly what
  // capabilities the user's machine has because the pexe to nexe
  // translation is specific to the machine, and CPU information goes
  // into the translation cache. This allows the translator to generate
  // faster code.
  //
  // Care must be taken to avoid instructions which aren't supported by
  // the NaCl sandbox. Ideally the translator would do this, but there's
  // no point in not doing the whitelist here.
  //
  // TODO(jfb) Some features are missing, either because the NaCl
  //           sandbox doesn't support them, because base::CPU doesn't
  //           detect them, or because they don't help vector shuffles
  //           (and we omit them because it simplifies testing). Add the
  //           other features.
  //
  // TODO(jfb) The following is x86-specific. The base::CPU class
  //           doesn't handle other architectures very well, and we
  //           should at least detect the presence of ARM's integer
  //           divide.
  std::vector<base::StringPiece> features;
  base::CPU cpu;

  // On x86, SSE features are ordered: the most recent one implies the
  // others. Care is taken here to only specify the latest SSE version,
  // whereas non-SSE features don't follow this model: POPCNT is
  // effectively always implied by SSE4.2 but has to be specified
  // separately.
  //
  // TODO: AVX2, AVX, SSE 4.2.
  if (cpu.has_sse41()) features.push_back("+sse4.1");
  // TODO: SSE 4A, SSE 4.
  else if (cpu.has_ssse3()) features.push_back("+ssse3");
  // TODO: SSE 3
  else if (cpu.has_sse2()) features.push_back("+sse2");

  if (cpu.has_popcnt()) features.push_back("+popcnt");

  // TODO: AES, LZCNT, ...
  return base::JoinString(features, ",");
}

}  // namespace nacl
