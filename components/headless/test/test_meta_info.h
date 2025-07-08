// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_TEST_TEST_META_INFO_H_
#define COMPONENTS_HEADLESS_TEST_TEST_META_INFO_H_

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/types/expected.h"

namespace headless {

// Holds headless JavaScript test meta info collected from '// META: ' comments
// found at the beginning of the .js test file. Meta info comment lines should
// contain one piece of info per line, with each line starting with '// META: '
// prefix. If the meta info line ends with '\', the next meta info line is
// considered to be a continuation of the previous meta info line.
struct TestMetaInfo {
  TestMetaInfo();
  TestMetaInfo(const TestMetaInfo& other);
  ~TestMetaInfo();

  bool operator==(const TestMetaInfo& other) const;

  // Command line switches, one switch per line.
  // META: --screen-info={1600x1200}
  base::flat_map<std::string, std::string> command_line_switches;

  // If true, Chrome Headless Mode and Headless Shell test expectations will be
  // maintained in '<script_name>-headless-mode-expected.txt' and
  // '<script_name>-headless-shell-expected.txt' respectively.
  bool fork_headless_mode_expectations = false;
  bool fork_headless_shell_expectations = false;

  static base::expected<TestMetaInfo, std::string> FromString(
      std::string_view test_body);

  bool IsEmpty() const;

  void AppendToCommandLine(base::CommandLine& command_line);
};

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_TEST_TEST_META_INFO_H_
