// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/content/renderer/autofill_assistant_agent_debug_utils.h"

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

namespace {

constexpr char kRoleLabelsJsonParameter[] = "roles";
constexpr char kObjectiveLabelsJsonParameter[] = "objectives";

}  // namespace

std::string NodeSignalsToDebugString(
    const blink::AutofillAssistantNodeSignals& node_signals) {
  std::ostringstream out;

  out << "AutofillAssistantNodeSignals {\n"
      << "\tbackend_node_id: " << node_signals.backend_node_id
      << "\n\tnode_features {";
  for (const auto& text : node_signals.node_features.text) {
    out << "\n\t\ttext: " << text.Utf16();
  }
  out << "\n\t\taria: " << node_signals.node_features.aria.Utf16()
      << "\n\t\thtml_tag: " << node_signals.node_features.html_tag.Utf16()
      << "\n\t\ttype: " << node_signals.node_features.type.Utf16()
      << "\n\t\tinvisible_attributes: "
      << node_signals.node_features.invisible_attributes.Utf16()
      << "\n\t}\n\tlabel_features {";
  for (const auto& text : node_signals.label_features.text) {
    out << "\n\t\ttext: " << text.Utf16();
  }
  out << "\n\t}\n\tcontext_features {";
  for (const auto& header_text : node_signals.context_features.header_text) {
    out << "\n\t\theader_text: " << header_text.Utf16();
  }
  out << "\n\t\tform_type: " << node_signals.context_features.form_type.Utf16()
      << "\n\t}\n}";

  return out.str();
}

SemanticLabelsPair DecodeSemanticPredictionLabelsJson(std::string encodedJson) {
  std::string decoded_json;
  base::Base64Decode(encodedJson, &decoded_json);
  absl::optional<base::Value> parsed_json =
      base::JSONReader::Read(decoded_json);

  SemanticPredictionLabelMap roles;
  SemanticPredictionLabelMap objectives;
  if (parsed_json.has_value() && parsed_json.value().is_dict()) {
    for (const auto [param, enum_list] : parsed_json->DictItems()) {
      if ((param != kRoleLabelsJsonParameter &&
           param != kObjectiveLabelsJsonParameter) ||
          !enum_list.is_list()) {
        continue;
      }
      auto* curr_map =
          (param == kRoleLabelsJsonParameter ? &roles : &objectives);
      for (const auto& enum_pair : enum_list.GetList()) {
        if (!enum_pair.is_dict() || enum_pair.DictSize() != 2) {
          continue;
        }

        const base::Value* id = enum_pair.GetDict().Find("id");
        const base::Value* name = enum_pair.GetDict().Find("name");
        if (!(id && id->is_int() && name && name->is_string())) {
          continue;
        }
        curr_map->try_emplace(id->GetInt(), name->GetString());
      }
    }
  }
  return std::make_pair(roles, objectives);
}

std::u16string SemanticPredictionResultToDebugString(
    SemanticPredictionLabelMap roles,
    SemanticPredictionLabelMap objectives,
    const ModelExecutorResult& result,
    bool ignore_objective) {
  int role = result.role;
  int objective = result.objective;
  if (!roles.empty() || !objectives.empty()) {
    std::string result_label =
        roles.contains(role) ? roles.at(role)
                             : "(missing-label) " + base::NumberToString(role);
    std::string objective_label =
        objectives.contains(objective)
            ? objectives.at(objective)
            : "(missing-label) " + base::NumberToString(objective);
    return base::StrCat(
        {u"{role: ", std::u16string(result_label.begin(), result_label.end()),
         u", objective: ",
         std::u16string(objective_label.begin(), objective_label.end()),
         (ignore_objective ? u"(ignored)}" : u"}"),
         (result.used_override ? u"[override]" : u"")});
  }

  return base::StrCat({u"{role: ", base::NumberToString16(role),
                       u", objective: ", base::NumberToString16(objective),
                       (ignore_objective ? u"(ignored)}" : u"}"),
                       (result.used_override ? u"[override]" : u"")});
}

}  // namespace autofill_assistant
