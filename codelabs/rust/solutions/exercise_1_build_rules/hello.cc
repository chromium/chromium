// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string>

#include "base/check.h"
#include "base/command_line.h"

extern "C" void hello_from_rust();

int main(int argc, char* argv[]) {
  CHECK(base::CommandLine::Init(argc, argv));
  printf("Hello from C++!\n");
  hello_from_rust();
}
