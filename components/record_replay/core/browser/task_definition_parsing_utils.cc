// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_definition_parsing_utils.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "url/gurl.h"

namespace record_replay {

namespace {

// Maps a single step dictionary from JSON to the protobuf StepDefinition.
base::expected<StepDefinition, std::string> MapStep(
    const base::DictValue& step_dict) {
  StepDefinition step_proto;
  const std::string* step_desc = step_dict.FindString("description");
  if (!step_desc || step_desc->empty()) {
    return base::unexpected("Missing required field 'description' in step.");
  }
  step_proto.set_description(*step_desc);

  const base::ListValue* keys_list = step_dict.FindList("expected_data_keys");
  if (keys_list) {
    for (const auto& key : *keys_list) {
      if (key.is_string()) {
        step_proto.add_expected_data_keys(key.GetString());
      }
    }
  }
  return step_proto;
}

// Parses a list of explicit steps (Detailed Syntax) and maps them to the proto.
base::expected<TaskDefinition, std::string> ParseDetailedSyntax(
    const GURL& url,
    const std::string& title,
    const std::string* instructions,
    const std::string* anchored_message,
    const base::ListValue& steps_list) {
  TaskDefinition task_definition;
  task_definition.set_url(url.spec());
  task_definition.set_title(title);

  if (const std::string* desc =
          instructions ? instructions : anchored_message) {
    task_definition.set_description(*desc);
  }

  int step_index = 1;  // Protobuf is 1-indexed.
  for (const base::Value& step_val : steps_list) {
    if (!step_val.is_dict()) {
      return base::unexpected(
          base::StrCat({"Step at index ", base::NumberToString(step_index - 1),
                        " is not a dictionary."}));
    }

    base::expected<StepDefinition, std::string> step =
        MapStep(step_val.GetDict());
    if (!step.has_value()) {
      return base::unexpected(
          base::StrCat({"Error in step ", base::NumberToString(step_index - 1),
                        ": ", step.error()}));
    }
    (*task_definition.mutable_steps())[step_index++] = std::move(step.value());
  }
  return task_definition;
}

// Synthesizes a single-step sequence using a fallback description.
base::expected<TaskDefinition, std::string> ParseQuickSyntax(
    const GURL& url,
    const std::string& title,
    const std::string& fallback_desc) {
  TaskDefinition task_definition;
  task_definition.set_url(url.spec());
  task_definition.set_title(title);
  task_definition.set_description(fallback_desc);

  StepDefinition step_proto;
  step_proto.set_description(fallback_desc);
  (*task_definition.mutable_steps())[1] = std::move(step_proto);

  return task_definition;
}

}  // namespace

// Parses a top-level task definition dictionary.
// Validates required fields and handles both Detailed and Quick syntax.
base::expected<TaskDefinition, std::string> ParseTaskDefinition(
    const base::DictValue& dict) {
  const std::string* url_str = dict.FindString("url");
  const std::string* title = dict.FindString("title");
  const std::string* instructions = dict.FindString("instructions");
  const std::string* anchored_message = dict.FindString("anchored_message");
  const base::ListValue* steps_list = dict.FindList("steps");

  // Intent Validation: Ensure the mandatory 'url' is present and forms a valid
  // GURL. This is critical because recordings are indexed and retrieved by URL.
  if (!url_str || url_str->empty()) {
    return base::unexpected("Missing required field 'url'.");
  }
  GURL url(*url_str);
  if (!url.is_valid()) {
    return base::unexpected(base::StrCat({"Invalid GURL: ", *url_str}));
  }

  // Intent Validation: 'title' is required for UI presentation of the task.
  if (!title || title->empty()) {
    return base::unexpected("Missing required field 'title'.");
  }

  bool has_steps = steps_list && !steps_list->empty();
  bool has_instructions = instructions && !instructions->empty();
  bool has_anchored_message = anchored_message && !anchored_message->empty();

  // Intent Validation: Require at least one description or steps with one.
  if (!has_steps && !has_instructions && !has_anchored_message) {
    return base::unexpected(
        "At least one of 'instructions', 'steps', or "
        "'anchored_message' must be present.");
  }

  return has_steps ? ParseDetailedSyntax(url, *title, instructions,
                                         anchored_message, *steps_list)
                   : ParseQuickSyntax(
                         url, *title,
                         has_instructions ? *instructions : *anchored_message);
}
}  // namespace record_replay
