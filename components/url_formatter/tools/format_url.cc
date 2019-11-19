// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This binary takes a list of domain names, tries to convert them to unicode
// and prints out the result. The list can be passed as a text file or via
// stdin. In both cases, the output is printed as (input_domain, output_domain)
// pairs on separate lines.

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "components/url_formatter/url_formatter.h"

void PrintUsage(const char* process_name) {
  std::cout << "Usage:" << std::endl;
  std::cout << process_name << " <file>" << std::endl;
  std::cout << std::endl;
  std::cout << "<file> is a text file with one hostname per line." << std::endl;
  std::cout << "Each hostname is converted to unicode, if safe. Otherwise, "
            << "it's printed unchanged." << std::endl;
}

void Convert(std::istream& input) {
  base::i18n::InitializeICU();
  for (std::string line; std::getline(input, line);) {
    std::cout << line << ", " << url_formatter::IDNToUnicode(line) << std::endl;
  }
}

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();

  if (cmd->HasSwitch("help")) {
    PrintUsage(argv[0]);
    return 0;
  }

  if (argc > 1) {
    const std::string filename = argv[1];
    std::ifstream input(filename);
    if (!input.good()) {
      LOG(ERROR) << "Could not open file " << filename;
      return -1;
    }
    Convert(input);
  } else {
    Convert(std::cin);
  }

  return EXIT_SUCCESS;
}
