// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The entrypoint for the starboard cast runtime. If loggy is enabled (typically
// used for partner builds), this code forks a separate process to run loggy for
// logging.
//
// Also contains logic for parsing chromium args passed as JSON (used by some
// partner platforms).

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chromecast/app/cast_main_delegate.h"
#include "chromecast/cast_core/child_log_process.h"
#include "content/public/app/content_main.h"

namespace {

constexpr char kHomeEnvOverride[] = "home-env-override";
constexpr char kRuntimeHomeSubdirUsedFile[] = ".dirty";
constexpr char kParametersKey[] = "parameters";
constexpr char kArgvKey[] = "argv";

// JSONArgsParser determines whether command line arguments were delivered
// as JSON or the standard format and, if necessary, parses the JSON
// arguments.
class JSONArgsParser {
 public:
  JSONArgsParser(int argc, const char** argv) {
    if (!TryParseJson(argc, argv)) {
      // If args were not provided as JSON, use defaults.
      for (int i = 0; i < argc; i++) {
        argv_.push_back(argv[i]);
      }
      argv_.push_back(nullptr);
    }
  }

  int argc() { return argv_.size() - 1; }

  const char** argv() { return argv_.data(); }

 private:
  // If the command is of the format `<command> <json>`, tries to parse
  // parameters from the JSON blob in |argv[1] instead of directly from
  // |argv|. Returns true if successful; otherwise, returns false.
  bool TryParseJson(int argc, const char** argv) {
    // Required format is `<command> <json>`
    if (argc != 2) {
      return false;
    }

    // JSON must be the following format. All keys and values are strings.
    // {"parameters":{"argv":["arg1", ...]}}
    std::string argv1 = std::string(argv[1]);
    std::optional<base::Value::Dict> root = base::JSONReader::ReadDict(argv1);
    if (!root) {
      // Try to fix unquoted JSON
      base::ReplaceSubstringsAfterOffset(&argv1, 0, "{", "{\"");
      base::ReplaceSubstringsAfterOffset(&argv1, 0, "[", "[\"");
      base::ReplaceSubstringsAfterOffset(&argv1, 0, "]", "\"]");
      base::ReplaceSubstringsAfterOffset(&argv1, 0, ":", "\":\"");
      base::ReplaceSubstringsAfterOffset(&argv1, 0, ",", "\",\"");
      base::ReplaceSubstringsAfterOffset(&argv1, 0, ":\"[", ":[");
      base::ReplaceSubstringsAfterOffset(&argv1, 0, ":\"{", ":{");

      // Special case to handle unix:/tmp. This means that things like
      // "valid_key_unix":"/valid_value" will fail to parse. Known issue.
      base::ReplaceSubstringsAfterOffset(&argv1, 0, "unix\":\"/", "unix:/");
      root = base::JSONReader::ReadDict(argv1);
    }

    if (!root) {
      return false;
    }

    base::Value::Dict* v = root->FindDict(kParametersKey);
    if (!v) {
      return false;
    }

    base::Value::List* argv_list = v->FindList(kArgvKey);
    if (!argv_list) {
      return false;
    }

    for (const auto& val : *argv_list) {
      // All values must be strings.
      if (!val.is_string()) {
        return false;
      }
      argv_parsed_.push_back(val.GetString());
    }

    // Parsing is successful, so load up |argv_| and return |true|.
    argv_.push_back(argv[0]);
    for (const std::string& arg : argv_parsed_) {
      argv_.push_back(arg.c_str());
    }
    argv_.push_back(nullptr);
    return true;
  }

  // Persists the backing memory of |argv|.
  std::vector<std::string> argv_parsed_;
  std::vector<const char*> argv_;
};

}  // namespace

int main(int argc, const char** argv) {
  JSONArgsParser args(argc, argv);

  chromecast::ForkAndRunLogProcessIfSpecified(args.argc(), args.argv());

  chromecast::shell::CastMainDelegate delegate;
  content::ContentMainParams params(&delegate);

  base::CommandLine temp_cmd(args.argc(), args.argv());
  std::string home_override = temp_cmd.GetSwitchValueASCII(kHomeEnvOverride);
  if (!home_override.empty()) {
    LOG(INFO) << "HOME variable was previously \"" << getenv("HOME")
              << "\"; overriding to \"" << home_override << "\".";
    setenv("HOME", home_override.c_str(), 1);

    base::FilePath home_directory(home_override);
    if (!base::DirectoryExists(home_directory)) {
      CHECK(base::CreateDirectory(home_directory));
    }

    base::FilePath home_directory_used_file =
        home_directory.AppendASCII(kRuntimeHomeSubdirUsedFile);
    if (!base::PathExists(home_directory_used_file)) {
      base::File(home_directory_used_file,
                 base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    }
  }

  params.argc = args.argc();
  params.argv = args.argv();

  return content::ContentMain(std::move(params));
}
