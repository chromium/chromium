// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a utility executable used for generating hashes for dll names
// for inclusion in tools/metrics/histograms/histograms.xml. Every
// dll name must have a corresponding entry in the enum there.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chrome/chrome_elf/dll_hash/dll_hash.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <dll name> <dll name> <...>\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "Prints hashes for dll names.\n");
    fprintf(stderr, "Example: %s \"my_dll.dll\" \"user32.dll\"\n", argv[0]);
    return 1;
  }
  for (int i = 1; i < argc; i++) {
    int hash = DllNameToHash(std::string(argv[i]));
    printf("<int value=\"%d\" label=\"%s\"/>\n", hash, argv[i]);
  }
  return 0;
}
