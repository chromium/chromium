// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_parser_json.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "components/update_client/protocol_definition.h"

namespace update_client {

namespace {

std::string GetValueString(const base::Value::Dict& node, const char* key) {
  const std::string* value = node.FindString(key);
  return value ? *value : std::string();
}

base::expected<std::string, std::string> Parse(const base::Value::Dict& node,
                                               const std::string& key) {
  const std::string* value = node.FindString(key);
  if (!value) {
    return base::unexpected(base::StrCat({"Missing ", key}));
  }
  return base::expected<std::string, std::string>(*value);
}

base::expected<base::Version, std::string> ParseVersion(
    const base::Value::Dict& node,
    const std::string& key) {
  base::expected<std::string, std::string> value = Parse(node, key);
  if (!value.has_value()) {
    return base::unexpected(value.error());
  }
  base::Version version(value.value());
  if (!version.IsValid()) {
    return base::unexpected(
        base::StrCat({"Invalid version: '", value.value(), "'."}));
  }
  return version;
}

std::optional<std::string> ParseOptional(const base::Value::Dict& node,
                                         const std::string& key) {
  const std::string* value = node.FindString(key);
  if (value) {
    return *value;
  }
  return std::nullopt;
}

int64_t ParseNumberWithDefault(const base::Value::Dict& node,
                               const std::string& key,
                               int64_t def) {
  const std::optional<double> value = node.FindDouble(key);
  if (value) {
    const double val = value.value();
    if (0 <= val && val < protocol_request::kProtocolMaxInt) {
      return static_cast<int64_t>(val);
    }
  }
  return def;
}

std::string ParseWithDefault(const base::Value::Dict& node,
                             const std::string& key,
                             const std::string& def) {
  const std::string* value = node.FindString(key);
  if (value) {
    return *value;
  }
  return def;
}

std::string ParseWithDefault(const base::Value::Dict& node,
                             const std::string& outer_key,
                             const std::string& inner_key,
                             const std::string& def) {
  const base::Value::Dict* outer = node.FindDict(outer_key);
  return outer ? ParseWithDefault(*outer, inner_key, def) : def;
}

base::expected<ProtocolParser::Operation, std::string> ParseOperation(
    const base::Value& node_val) {
  if (!node_val.is_dict()) {
    return base::unexpected("'operation' contains a non-dictionary.");
  }
  const base::Value::Dict& node = node_val.GetDict();
  ProtocolParser::Operation op;
  base::expected<std::string, std::string> type = Parse(node, "type");
  if (!type.has_value()) {
    return base::unexpected(type.error());
  }
  op.type = type.value();
  op.sha256_out = ParseWithDefault(node, "out", "sha256", {});
  op.sha256_in = ParseWithDefault(node, "in", "sha256", {});
  op.sha256_previous = ParseWithDefault(node, "previous", "sha256", {});
  op.path = ParseWithDefault(node, "path", {});
  op.arguments = ParseWithDefault(node, "arguments", {});
  op.size = ParseNumberWithDefault(node, "size", 0);
  if (const base::Value::List* list = node.FindList("urls")) {
    for (const base::Value& url_node : *list) {
      if (!url_node.is_dict()) {
        return base::unexpected("url node is not a dict");
      }
      base::expected<std::string, std::string> url =
          Parse(url_node.GetDict(), "url");
      if (!url.has_value()) {
        return base::unexpected(url.error());
      }
      GURL gurl(url.value());
      if (!gurl.is_valid()) {
        return base::unexpected("operation contains a malformed url");
      }
      op.urls.push_back(gurl);
    }
  }
  return op;
}

base::expected<ProtocolParser::Pipeline, std::string> ParsePipeline(
    const base::Value& node_val) {
  if (!node_val.is_dict()) {
    return base::unexpected("'pipeline' contains a non-dictionary.");
  }
  ProtocolParser::Pipeline pipeline;
  pipeline.pipeline_id =
      ParseWithDefault(node_val.GetDict(), "pipeline_id", {});
  if (const base::Value::List* node =
          node_val.GetDict().FindList("operations")) {
    for (const base::Value& operation_node : *node) {
      base::expected<ProtocolParser::Operation, std::string> operation =
          ParseOperation(operation_node);
      if (!operation.has_value()) {
        return base::unexpected(operation.error());
      }
      pipeline.operations.push_back(operation.value());
    }
  }
  return pipeline;
}

void ParseData(const base::Value& data_node_val, ProtocolParser::App* result) {
  if (!data_node_val.is_dict()) {
    return;
  }
  const base::Value::Dict& data_node = data_node_val.GetDict();

  result->data.emplace_back(
      GetValueString(data_node, "index"), GetValueString(data_node, "#text"));
}

bool ParseUpdateCheck(const base::Value* node_val,
                      ProtocolParser::App* result,
                      std::string* error) {
  if (!node_val || !node_val->is_dict()) {
    *error = "'updatecheck' node is missing or not a dictionary.";
    return false;
  }
  const base::Value::Dict& node = node_val->GetDict();

  for (auto [k, v] : node) {
    if (!k.empty() && k.front() == '_' && v.is_string()) {
      result->custom_attributes[k] = v.GetString();
    }
  }

  // result->status was set to "ok" when parsing the app node; overwrite it with
  // the updatecheck status.
  base::expected<std::string, std::string> status = Parse(node, "status");
  if (!status.has_value()) {
    *error = status.error();
    return false;
  }
  result->status = status.value();

  if (result->status == "noupdate") {
    return true;
  }

  if (result->status == "ok") {
    base::expected<base::Version, std::string> nextversion =
        ParseVersion(node, "nextversion");
    if (nextversion.has_value()) {
      result->nextversion = nextversion.value();
    } else {
      *error = nextversion.error();
      return false;
    }

    if (const base::Value::List* list = node.FindList("pipelines")) {
      for (const base::Value& pipeline_node : *list) {
        base::expected<ProtocolParser::Pipeline, std::string> pipeline =
            ParsePipeline(pipeline_node);
        if (!pipeline.has_value()) {
          *error = pipeline.error();
          return false;
        }
        result->pipelines.push_back(pipeline.value());
      }
    }
    return true;
  }

  // Return the |updatecheck| element status as a parsing error.
  *error = result->status;
  return true;
}

bool ParseApp(const base::Value& node_value,
              ProtocolParser::App* result,
              std::string* error) {
  if (!node_value.is_dict()) {
    *error = "'app' is not a dictionary.";
    return false;
  }
  const base::Value::Dict& node = node_value.GetDict();

  result->cohort = ParseOptional(node, "cohort");
  result->cohort_name = ParseOptional(node, "cohortname");
  result->cohort_hint = ParseOptional(node, "cohorthint");
  base::expected<std::string, std::string> appid = Parse(node, "appid");
  if (!appid.has_value()) {
    *error = appid.error();
    return false;
  }
  result->app_id = appid.value();
  base::expected<std::string, std::string> status = Parse(node, "status");
  if (!status.has_value()) {
    *error = status.error();
    return false;
  }
  result->status = status.value();

  if (result->status == "ok") {
    if (const base::Value::List* data_node = node.FindList("data")) {
      std::ranges::for_each(*data_node, [&result](const base::Value& data) {
        ParseData(data, result);
      });
    }
    return ParseUpdateCheck(node.Find("updatecheck"), result, error);
  }

  return true;
}

}  // namespace

bool ProtocolParserJSON::DoParse(const std::string& response_json,
                                 Results* results) {
  CHECK(results);

  if (response_json.empty()) {
    ParseError("Empty JSON.");
    return false;
  }

  // The JSON response contains a prefix to prevent XSSI.
  static constexpr char kJSONPrefix[] = ")]}'";
  if (!base::StartsWith(response_json, kJSONPrefix,
                        base::CompareCase::SENSITIVE)) {
    ParseError("Missing secure JSON prefix.");
    return false;
  }
  const auto doc = base::JSONReader::ReadDict(base::MakeStringPiece(
      response_json.begin() + std::char_traits<char>::length(kJSONPrefix),
      response_json.end()));
  if (!doc) {
    ParseError("JSON read error.");
    return false;
  }
  const base::Value::Dict* response_node = doc->FindDict("response");
  if (!response_node) {
    ParseError("Missing 'response' element or 'response' is not a dictionary.");
    return false;
  }
  const std::string* protocol = response_node->FindString("protocol");
  if (!protocol) {
    ParseError("Missing/non-string protocol.");
    return false;
  }
  if (*protocol != protocol_request::kProtocolVersion) {
    ParseError("Incorrect protocol. (expected '%s', found '%s')",
               protocol_request::kProtocolVersion, protocol->c_str());
    return false;
  }

  const base::Value::Dict* daystart_node = response_node->FindDict("daystart");
  if (daystart_node) {
    const std::optional<int> elapsed_days =
        daystart_node->FindInt("elapsed_days");
    if (elapsed_days) {
      results->daystart_elapsed_days = *elapsed_days;
    }
  }

  const base::Value::List* app_node = response_node->FindList("apps");
  if (app_node) {
    for (const auto& app : *app_node) {
      App result;
      std::string error;
      if (ParseApp(app, &result, &error)) {
        results->apps.push_back(result);
      } else {
        ParseError("%s", error.c_str());
      }
    }
  }

  return true;
}

base::expected<ProtocolParser::Results, std::string>
ProtocolParserJSON::ParseJSON(const std::string& json) {
  ProtocolParserJSON parser;
  if (parser.Parse(json)) {
    return parser.results();
  }
  return base::unexpected(parser.errors());
}

}  // namespace update_client
