// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/auto_start_linux.h"

#include <stddef.h>

#include <memory>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"

namespace {

const base::FilePath::CharType kAutostart[] = "autostart";

}  // namespace

// static
bool AutoStart::AddApplication(const std::string& autostart_filename,
                               const std::string& application_name,
                               const std::string& command_line,
                               bool is_terminal_app) {
  std::unique_ptr<base::Environment> environment(base::Environment::Create());
  base::FilePath autostart_directory = GetAutostartDirectory(environment.get());
  if (!base::DirectoryExists(autostart_directory) &&
      !base::CreateDirectory(autostart_directory)) {
    return false;
  }

  base::FilePath autostart_file =
      autostart_directory.Append(autostart_filename);
  std::string terminal = is_terminal_app ? "true" : "false";
  std::string autostart_file_contents =
      "[Desktop Entry]\n"
      "Type=Application\n"
      "Terminal=" + terminal + "\n"
      "Exec=" + command_line + "\n"
      "Name=" + application_name + "\n";
  if (!base::WriteFile(autostart_file, autostart_file_contents)) {
    base::DeleteFile(autostart_file);
    return false;
  }
  return true;
}

// static
bool AutoStart::Remove(const std::string& autostart_filename) {
  std::unique_ptr<base::Environment> environment(base::Environment::Create());
  base::FilePath autostart_directory = GetAutostartDirectory(environment.get());
  base::FilePath autostart_file =
      autostart_directory.Append(autostart_filename);
  return base::DeleteFile(autostart_file);
}

// static
bool AutoStart::GetAutostartFileContents(
    const std::string& autostart_filename, std::string* contents) {
  std::unique_ptr<base::Environment> environment(base::Environment::Create());
  base::FilePath autostart_directory = GetAutostartDirectory(environment.get());
  base::FilePath autostart_file =
      autostart_directory.Append(autostart_filename);
  return base::ReadFileToString(autostart_file, contents);
}

// static
bool AutoStart::GetAutostartFileValue(const std::string& autostart_filename,
                                      const std::string& value_name,
                                      std::string* value) {
  std::string contents;
  if (!GetAutostartFileContents(autostart_filename, &contents))
    return false;
  base::StringTokenizer tokenizer(contents, "\n");
  std::string token = value_name + "=";
  while (tokenizer.GetNext()) {
    if (base::StartsWith(tokenizer.token_piece(), token)) {
      *value = std::string(tokenizer.token_piece().substr(token.length()));
      return true;
    }
  }
  return false;
}

// static
base::FilePath AutoStart::GetAutostartDirectory(
    base::Environment* environment) {
  base::FilePath result = base::nix::GetXDGDirectory(
      environment, base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir);
  result = result.Append(kAutostart);
  return result;
}
