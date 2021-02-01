// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "url/gurl.h"

namespace {

// Print the command line help.
void PrintHelp() {
  LOG(ERROR) << "schema_org_name_generator <schema-file> ... <schema-file>"
             << " <output-file>";
}

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

#if defined(OS_WIN)
  std::vector<std::string> args;
  base::CommandLine::StringVector wide_args = command_line.GetArgs();
  for (const auto& arg : wide_args) {
    args.push_back(base::WideToUTF8(arg));
  }
#else
  base::CommandLine::StringVector args = command_line.GetArgs();
#endif
  if (args.size() < 2U) {
    PrintHelp();
    return 1;
  }

  // Read all the args and convert to file paths.
  std::vector<base::FilePath> paths;
  for (auto& arg : args) {
    paths.push_back(base::FilePath::FromUTF8Unsafe(arg));
  }

  // Check we have at least two paths.
  if (paths.size() < 2U) {
    PrintHelp();
    return 1;
  }

  // Get the last path which is the output file.
  base::FilePath output_path = paths.back();
  paths.pop_back();

  base::DictionaryValue output_map;
  std::set<std::string> names_to_generate;

  for (auto& path : paths) {
    path = base::MakeAbsoluteFilePath(path);
    if (!base::PathExists(path)) {
      LOG(ERROR) << "Input JSON file doesn't exist.";
      return 1;
    }

    std::string json_input;
    if (!base::ReadFileToString(path, &json_input)) {
      LOG(ERROR) << "Could not read input JSON file.";
      return 1;
    }

    auto value = base::JSONReader::Read(json_input);
    base::DictionaryValue* dict_value = nullptr;
    if (!value.has_value() || !value->GetAsDictionary(&dict_value)) {
      LOG(ERROR) << "Could not parse the input JSON file";
      return 1;
    }

    const base::ListValue* graph = nullptr;
    if (!dict_value->GetList("@graph", &graph)) {
      LOG(ERROR) << "Could not parse the @graph in the input JSON";
      return 1;
    }

    for (size_t i = 0; i < graph->GetSize(); ++i) {
      const base::DictionaryValue* parsed = nullptr;
      if (!graph->GetDictionary(i, &parsed)) {
        LOG(ERROR) << "Could not parse entry " << i << " in the input JSON";
        return 1;
      }

      std::string id;
      if (!parsed->GetString("@id", &id)) {
        LOG(ERROR) << "Could not extract the id from the entry";
        return 1;
      }

      if (id.empty()) {
        LOG(ERROR) << "ID was empty";
        return 1;
      }

      names_to_generate.insert(id);
      names_to_generate.insert(GURL(id).path().substr(1));
    }
  }

  std::set<unsigned> generated_hashes;
  for (auto& name : names_to_generate) {
    auto hash = base::PersistentHash(name);

    if (base::Contains(generated_hashes, hash)) {
      LOG(ERROR) << "Hash collision: " << name;
      return 1;
    }

    output_map.SetStringKey(name, base::StringPrintf("0x%x", hash));
    generated_hashes.insert(hash);
  }

  std::string output;
  if (!base::JSONWriter::Write(output_map, &output)) {
    LOG(ERROR) << "Failed to convert output to JSON.";
    return 1;
  }

  if (base::WriteFile(output_path, output.c_str(),
                      static_cast<uint32_t>(output.size())) <= 0) {
    LOG(ERROR) << "Failed to write output.";
    return 1;
  }

  return 0;
}
