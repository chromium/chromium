// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/platform_utils.h"

#include "base/containers/flat_set.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "components/device_signals/core/common/common_types.h"

namespace device_signals {

namespace {

constexpr base::FilePath::CharType kTilde = '~';
constexpr base::FilePath::CharType kEnvVariablePrefix = '$';

bool StringStartsWith(const base::FilePath::StringType& path_string,
                      const base::FilePath::CharType character) {
  return !path_string.empty() && path_string[0] == character;
}

bool PathStartsWith(const base::FilePath& file_path,
                    const base::FilePath::CharType character) {
  return !file_path.empty() && StringStartsWith(file_path.value(), character);
}

base::FilePath CreatePathFromComponents(
    const std::vector<base::FilePath::StringType>& components) {
  if (components.empty()) {
    return base::FilePath();
  }

  base::FilePath new_path(components[0]);
  for (size_t i = 1U; i < components.size(); ++i) {
    new_path = new_path.Append(components[i]);
  }
  return new_path;
}

}  // namespace

bool ResolvePath(const base::FilePath& file_path,
                 base::FilePath* resolved_file_path) {
  auto environment = base::Environment::Create();
  // Expand the first component of the path if it is either a tilde or an
  // environment variable.
  base::FilePath expanded_file_path = file_path;
  bool starts_with_tilde = PathStartsWith(expanded_file_path, kTilde);
  bool starts_with_env_variable =
      PathStartsWith(expanded_file_path, kEnvVariablePrefix);

  if (starts_with_tilde || starts_with_env_variable) {
    base::flat_set<base::FilePath::StringType> visited_env_variables;
    auto path_components = expanded_file_path.GetComponents();

    // Use a loop to handle cases where an environment variable resolves to
    // another one, or to the tilde. Make use of a set to ensure that the env
    // variables are not cyclic (hence triggering an infinite loop).
    while (starts_with_tilde || starts_with_env_variable) {
      if (starts_with_tilde) {
        path_components[0].replace(
            0U, 1U, base::StringPrintf("$%s", base::env_vars::kHome));
      } else if (starts_with_env_variable) {
        auto env_variable_name = path_components[0].substr(1U);

        if (visited_env_variables.contains(env_variable_name)) {
          // Environment variables have a cyclic dependency.
          return false;
        } else {
          visited_env_variables.insert(env_variable_name);
        }

        if (!environment->GetVar(env_variable_name, &path_components[0])) {
          return false;
        }
      }

      starts_with_tilde = StringStartsWith(path_components[0], kTilde);
      starts_with_env_variable =
          StringStartsWith(path_components[0], kEnvVariablePrefix);
    }

    expanded_file_path = CreatePathFromComponents(path_components);
  }

  // Resolve any relative path traversals that may exist (e.g. "..");
  base::FilePath local_resolved_file_path =
      base::MakeAbsoluteFilePath(expanded_file_path);
  if (local_resolved_file_path.empty()) {
    return false;
  }

  *resolved_file_path = local_resolved_file_path;
  return true;
}

std::optional<base::FilePath> GetProcessExePath(base::ProcessId pid) {
  auto file_path = base::GetProcessExecutablePath(pid);
  if (file_path.empty()) {
    return std::nullopt;
  }
  return file_path;
}

std::optional<CrowdStrikeSignals> GetCrowdStrikeSignals() {
  return std::nullopt;
}

}  // namespace device_signals
