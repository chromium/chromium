// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/strings/string_util.h"
#include "build/build_config.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include "base/cpu.h"
#endif

namespace nacl {

const char* GetSandboxArch() {
#if defined(ARCH_CPU_ARM_FAMILY)
  return "arm";
#elif defined(ARCH_CPU_MIPS_FAMILY)
  return "mips32";
#elif defined(ARCH_CPU_X86_FAMILY)

#if ARCH_CPU_64_BITS
  return "x86-64";
#else
  return "x86-32";
#endif  // ARCH_CPU_64_BITS

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
  // no point in not doing the allowlist here.
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
  std::vector<std::string_view> features;
#if defined(ARCH_CPU_X86_FAMILY)
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
#endif

  return base::JoinString(features, ",");
}

}  // namespace nacl
