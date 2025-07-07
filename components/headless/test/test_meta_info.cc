// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/test/test_meta_info.h"

#include "base/strings/string_util.h"
#include "third_party/re2/src/re2/re2.h"

using re2::RE2;

namespace headless {

namespace {

constexpr char kForkHeadlessModeExpectations[] =
    "fork_headless_mode_expectations";
constexpr char kForkHeadlessShellExpectations[] =
    "fork_headless_shell_expectations";

constexpr char kInvalidMetaInfo[] = "Invalid meta info: ";

std::vector<std::string> CollectMetaInfos(std::string_view test_body) {
  std::vector<std::string> meta_infos;

  // Match '// META:' then grab everything until the newline.
  static RE2 re(R"(// META:(.*))");
  while (!test_body.empty()) {
    std::string meta_info;
    if (!RE2::FindAndConsume(&test_body, re, &meta_info)) {
      break;
    }

    base::TrimWhitespaceASCII(meta_info, base::TRIM_ALL, &meta_info);

    if (!meta_infos.empty() && meta_infos.back().back() == '\\') {
      meta_infos.back().pop_back();
      meta_infos.back().append(meta_info);
    } else {
      meta_infos.push_back(std::move(meta_info));
    }
  }

  return meta_infos;
}

}  // namespace

TestMetaInfo::TestMetaInfo() = default;
TestMetaInfo::TestMetaInfo(const TestMetaInfo& other) = default;
TestMetaInfo::~TestMetaInfo() = default;

bool TestMetaInfo::operator==(const TestMetaInfo& other) const = default;

// static
base::expected<TestMetaInfo, std::string> TestMetaInfo::FromString(
    std::string_view test_body) {
  TestMetaInfo result;

  for (const std::string& meta_info : CollectMetaInfos(test_body)) {
    std::string name;
    std::string value;
    static RE2 re_command_line_switch(R"(--([\w-]+)(?:=(.*))?)");
    if (RE2::FullMatch(meta_info, re_command_line_switch, &name, &value)) {
      result.command_line_switches[name] = value;
      continue;
    }

    if (meta_info == kForkHeadlessModeExpectations) {
      result.fork_headless_mode_expectations = true;
      continue;
    }

    if (meta_info == kForkHeadlessShellExpectations) {
      result.fork_headless_shell_expectations = true;
      continue;
    }

    return base::unexpected(kInvalidMetaInfo + meta_info);
  }

  return base::ok(result);
}

bool TestMetaInfo::IsEmpty() const {
  return *this == TestMetaInfo();
}

void TestMetaInfo::AppendToCommandLine(base::CommandLine& command_line) {
  for (const auto& [name, value] : command_line_switches) {
    if (!value.empty()) {
      command_line.AppendSwitchASCII(name, value);
    } else {
      command_line.AppendSwitch(name);
    }
  }
}

}  // namespace headless
