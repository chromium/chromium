// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string>

#include "base/command_line.h"
#include "base/logging.h"

int main(int argc, char* argv[]) {
  CHECK(base::CommandLine::Init(argc, argv));

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  std::string greeting = command_line.GetSwitchValueASCII("greeting");
  if (greeting.empty()) {
    greeting = "Hello";
  }

  std::string name = command_line.GetSwitchValueASCII("name");
  if (name.empty()) {
    name = "world";
  }

  CHECK_GT(printf("%s, %s!\n", greeting.c_str(), name.c_str()), 0);
  LOG(INFO) << greeting << ", " << name << "!";
}