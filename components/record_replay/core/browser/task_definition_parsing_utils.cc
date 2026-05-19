// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/task_definition_parsing_utils.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "url/gurl.h"

namespace record_replay {

namespace {

base::expected<TaskParameter, std::string> MapParameter(
    const base::DictValue& dict) {
  // Intent: Extract parameter-level metadata properties from a seed parameter
  // dictionary. Map required keys ('key', 'name', 'type') and optional
  // descriptions to protobuf TaskParameter fields.
  TaskParameter param;
  const std::string* key = dict.FindString("key");
  if (!key || key->empty()) {
    return base::unexpected("Missing required field 'key' in parameter.");
  }
  param.set_key(*key);

  const std::string* name = dict.FindString("name");
  if (!name || name->empty()) {
    return base::unexpected("Missing required field 'name' in parameter.");
  }
  param.set_name(*name);

  const std::string* type = dict.FindString("type");
  if (!type || type->empty()) {
    return base::unexpected("Missing required field 'type' in parameter.");
  }
  param.set_type(*type);

  const std::string* desc = dict.FindString("description");
  if (desc) {
    param.set_description(*desc);
  }

  return param;
}

base::expected<TaskStep, std::string> MapStep(const base::DictValue& dict,
                                              int default_step_index,
                                              const GURL& default_url) {
  // Intent: Map a step-level dictionary from a JSON seed file to a protobuf
  // TaskStep. Validates sequence step properties ('step_index', 'url',
  // 'description') and recurses to parse the step-specific list of
  // placeholders/parameters.
  TaskStep step;
  std::optional<int> index = dict.FindInt("step_index");
  step.set_step_index(index ? *index : default_step_index);

  const std::string* desc = dict.FindString("description");
  if (!desc || desc->empty()) {
    return base::unexpected("Missing required field 'description' in step.");
  }
  step.set_description(*desc);

  const std::string* url = dict.FindString("url");
  step.set_url(url && !url->empty() ? *url : default_url.spec());

  const base::ListValue* params_list = dict.FindList("parameters");
  if (params_list) {
    int p_idx = 0;
    for (const base::Value& val : *params_list) {
      if (!val.is_dict()) {
        return base::unexpected(
            base::StrCat({"Parameter at index ", base::NumberToString(p_idx),
                          " in step is not a dictionary."}));
      }
      base::expected<TaskParameter, std::string> param =
          MapParameter(val.GetDict());
      if (!param.has_value()) {
        return base::unexpected(
            base::StrCat({"Error in parameter ", base::NumberToString(p_idx),
                          ": ", param.error()}));
      }
      *step.add_parameters() = std::move(param.value());
      p_idx++;
    }
  }
  return step;
}

base::expected<TaskDefinition, std::string> ParseDetailedSyntax(
    const GURL& url,
    const std::string& title,
    const std::string* description,
    const base::ListValue& steps_list) {
  TaskDefinition definition;
  definition.set_url(url.spec());
  definition.set_title(title);
  if (description) {
    definition.set_description(*description);
  }

  int step_index = 1;
  for (const base::Value& step_val : steps_list) {
    if (!step_val.is_dict()) {
      return base::unexpected(
          base::StrCat({"Step at index ", base::NumberToString(step_index),
                        " is not a dictionary."}));
    }

    const base::DictValue& step_dict = step_val.GetDict();
    base::expected<TaskStep, std::string> step =
        MapStep(step_dict, step_index, url);
    if (!step.has_value()) {
      return base::unexpected(
          base::StrCat({"Error in step ", base::NumberToString(step_index),
                        ": ", step.error()}));
    }
    *definition.add_task_steps() = step.value();

    StepDefinition legacy_step;
    legacy_step.set_description(step.value().description());

    const base::ListValue* keys_list = step_dict.FindList("expected_data_keys");
    if (keys_list) {
      for (const base::Value& key : *keys_list) {
        if (key.is_string()) {
          legacy_step.add_expected_data_keys(key.GetString());
        }
      }
    } else {
      for (int i = 0; i < step.value().parameters_size(); ++i) {
        legacy_step.add_expected_data_keys(step.value().parameters(i).key());
      }
    }
    (*definition.mutable_steps())[step_index] = legacy_step;

    step_index++;
  }
  return definition;
}

base::expected<TaskDefinition, std::string> ParseQuickSyntax(
    const GURL& url,
    const std::string& title,
    const std::string& fallback_desc) {
  TaskDefinition definition;
  definition.set_url(url.spec());
  definition.set_title(title);
  definition.set_description(fallback_desc);

  TaskStep* step = definition.add_task_steps();
  step->set_step_index(0);
  step->set_description(fallback_desc);
  step->set_url(url.spec());

  StepDefinition legacy_step;
  legacy_step.set_description(fallback_desc);
  (*definition.mutable_steps())[1] = legacy_step;

  return definition;
}

}  // namespace

base::expected<TaskDefinition, std::string> ParseTaskDefinition(
    const base::DictValue& dict) {
  const std::string* url_str = dict.FindString("url");
  const std::string* title = dict.FindString("title");
  const std::string* instructions = dict.FindString("instructions");
  const std::string* anchored_message = dict.FindString("anchored_message");
  const std::string* description = dict.FindString("description");
  const base::ListValue* steps_list = dict.FindList("steps");

  if (!url_str || url_str->empty()) {
    return base::unexpected("Missing required field 'url'.");
  }
  GURL url(*url_str);
  if (!url.is_valid()) {
    return base::unexpected(base::StrCat({"Invalid GURL: ", *url_str}));
  }

  if (!title || title->empty()) {
    return base::unexpected("Missing required field 'title'.");
  }

  const std::string* desc_val = nullptr;
  if (description && !description->empty()) {
    desc_val = description;
  } else if (instructions && !instructions->empty()) {
    desc_val = instructions;
  } else if (anchored_message && !anchored_message->empty()) {
    desc_val = anchored_message;
  }

  bool has_steps = steps_list && !steps_list->empty();

  if (!has_steps && !desc_val) {
    return base::unexpected(
        "At least one of 'description', 'instructions', 'anchored_message', "
        "or 'steps' must be present.");
  }

  return has_steps ? ParseDetailedSyntax(url, *title, desc_val, *steps_list)
                   : ParseQuickSyntax(url, *title, *desc_val);
}

}  // namespace record_replay
