// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fuchsia_component_support/append_arguments_from_file.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"

namespace fuchsia_component_support {

bool AppendArgumentsFromFile(const base::FilePath& path,
                             base::CommandLine& command_line) {
  JSONFileValueDeserializer json_deserializer(path, base::JSON_PARSE_RFC);
  int error_code = JSONFileValueDeserializer::JSON_NO_ERROR;
  std::unique_ptr<base::Value> value =
      json_deserializer.Deserialize(&error_code, nullptr);
  if (!value) {
    return error_code == JSONFileValueDeserializer::JSON_NO_SUCH_FILE;
  }
  base::Value::Dict* const dict = value->GetIfDict();
  if (!dict) {
    return false;
  }

  if (auto* const argv_list = dict->FindList("argv"); argv_list) {
    base::CommandLine::StringVector argv;
    argv.reserve(argv_list->size() + 1);
    argv.emplace_back();  // The program name.
    for (auto& arg_value : *argv_list) {
      if (auto* arg = arg_value.GetIfString(); arg) {
        argv.push_back(std::move(*arg));
      } else {
        return false;
      }
    }
    if (argv.size() > 1) {
      command_line.AppendArguments(base::CommandLine(argv),
                                   /*include_program=*/false);
    }
  }

  return true;
}

}  // namespace fuchsia_component_support
