// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a utility executable used for generating hashes for dll names
// for inclusion in tools/metrics/histograms/histograms.xml. Every
// dll name must have a corresponding entry in the enum there.

#include <stdio.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "chrome/chrome_elf/dll_hash/dll_hash.h"

int main(int argc, char** argv) {
  // SAFETY: argv has size argc, guaranteed by the OS.
  auto args = base::ToVector<std::string>(
      UNSAFE_BUFFERS(base::span(argv, static_cast<size_t>(argc))));

  if (args.size() < 2) {
    fprintf(stderr, "Usage: %s <dll name> <dll name> <...>\n", args[0].c_str());
    fprintf(stderr, "\n");
    fprintf(stderr, "Prints hashes for dll names.\n");
    fprintf(stderr, "Example: %s \"my_dll.dll\" \"user32.dll\"\n",
            args[0].c_str());
    return 1;
  }
  for (const auto& arg : base::span(args).subspan(1u)) {
    int hash = DllNameToHash(arg);
    printf("<int value=\"%d\" label=\"%s\"/>\n", hash, arg.c_str());
  }
  return 0;
}
