// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_parser_json.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "components/update_client/protocol_definition.h"

namespace update_client {

namespace {

std::string GetValueString(const base::Value::Dict& node, const char* key) {
  const std::string* value = node.FindString(key);
  return value ? *value : std::string();
}

bool ParseManifest(const base::Value& manifest_node_val,
                   ProtocolParser::Result* result,
                   std::string* error) {
  if (!manifest_node_val.is_dict()) {
    *error = "'manifest' is not a dictionary.";
    return false;
  }
  const base::Value::Dict& manifest_node = manifest_node_val.GetDict();
  const std::string* version = manifest_node.FindString("version");
  if (!version) {
    *error = "Missing version for manifest.";
    return false;
  }

  result->manifest.version = *version;
  if (!base::Version(result->manifest.version).IsValid()) {
    *error =
        base::StrCat({"Invalid version: '", result->manifest.version, "'."});
    return false;
  }

  // Get the optional minimum browser version.
  const std::string* browser_min_version =
      manifest_node.FindString("prodversionmin");
  if (browser_min_version) {
    result->manifest.browser_min_version = *browser_min_version;
    if (!base::Version(result->manifest.browser_min_version).IsValid()) {
      *error = base::StrCat({"Invalid prodversionmin: '",
                             result->manifest.browser_min_version, "'."});
      return false;
    }
  }

  result->manifest.run = GetValueString(manifest_node, "run");
  result->manifest.arguments = GetValueString(manifest_node, "arguments");

  const base::Value::Dict* packages_node = manifest_node.FindDict("packages");
  if (!packages_node) {
    *error = "Missing packages in manifest or 'packages' is not a dictionary.";
    return false;
  }
  const base::Value::List* package_node = packages_node->FindList("package");
  if (!package_node) {
    *error = "Missing package in packages.";
    return false;
  }

  for (const auto& package_val : *package_node) {
    if (!package_val.is_dict()) {
      *error = "'package' is not a dictionary.";
      return false;
    }
    const base::Value::Dict& package = package_val.GetDict();
    ProtocolParser::Result::Manifest::Package p;
    const std::string* name = package.FindString("name");
    if (!name) {
      *error = "Missing name for package.";
      return false;
    }
    p.name = *name;

    p.namediff = GetValueString(package, "namediff");
    p.fingerprint = GetValueString(package, "fp");
    p.hash_sha256 = GetValueString(package, "hash_sha256");
    p.hashdiff_sha256 = GetValueString(package, "hashdiff_sha256");

    const std::optional<double> size = package.FindDouble("size");
    if (size) {
      const double val = size.value();
      if (0 <= val && val < protocol_request::kProtocolMaxInt) {
        p.size = val;
      }
    }

    const std::optional<double> sizediff = package.FindDouble("sizediff");
    if (sizediff) {
      const double val = sizediff.value();
      if (0 <= val && val < protocol_request::kProtocolMaxInt) {
        p.sizediff = val;
      }
    }

    result->manifest.packages.push_back(std::move(p));
  }

  return true;
}

void ParseActions(const base::Value& actions_node,
                  ProtocolParser::Result* result) {
  if (!actions_node.is_dict()) {
    return;
  }

  const base::Value::List* action_node =
      actions_node.GetDict().FindList("action");
  if (!action_node) {
    return;
  }

  const base::Value::List& action_list = *action_node;
  if (action_list.empty() || !action_list[0].is_dict()) {
    return;
  }

  result->action_run = GetValueString(action_list[0].GetDict(), "run");
}

bool ParseUrls(const base::Value& urls_node_val,
               ProtocolParser::Result* result,
               std::string* error) {
  if (!urls_node_val.is_dict()) {
    *error = "'urls' is not a dictionary.";
    return false;
  }
  const base::Value::Dict& urls_node = urls_node_val.GetDict();
  const base::Value::List* url_node = urls_node.FindList("url");
  if (!url_node) {
    *error = "Missing url on urls.";
    return false;
  }

  for (const base::Value& url_val : *url_node) {
    if (!url_val.is_dict()) {
      continue;
    }
    const base::Value::Dict& url = url_val.GetDict();
    const std::string* codebase = url.FindString("codebase");
    if (codebase) {
      GURL crx_url(*codebase);
      if (crx_url.is_valid()) {
        result->crx_urls.push_back(std::move(crx_url));
      }
    }
    const std::string* codebasediff = url.FindString("codebasediff");
    if (codebasediff) {
      GURL crx_diffurl(*codebasediff);
      if (crx_diffurl.is_valid()) {
        result->crx_diffurls.push_back(std::move(crx_diffurl));
      }
    }
  }

  // Expect at least one url for full update.
  if (result->crx_urls.empty()) {
    *error = "Missing valid url for full update.";
    return false;
  }

  return true;
}

void ParseData(const base::Value& data_node_val,
               ProtocolParser::Result* result) {
  if (!data_node_val.is_dict()) {
    return;
  }
  const base::Value::Dict& data_node = data_node_val.GetDict();

  result->data.emplace_back(
      GetValueString(data_node, "status"), GetValueString(data_node, "name"),
      GetValueString(data_node, "index"), GetValueString(data_node, "#text"));
}

bool ParseUpdateCheck(const base::Value& updatecheck_node_val,
                      ProtocolParser::Result* result,
                      std::string* error) {
  if (!updatecheck_node_val.is_dict()) {
    *error = "'updatecheck' is not a dictionary.";
    return false;
  }
  const base::Value::Dict& updatecheck_node = updatecheck_node_val.GetDict();

  for (auto [k, v] : updatecheck_node) {
    if (!k.empty() && k.front() == '_' && v.is_string()) {
      result->custom_attributes[k] = v.GetString();
    }
  }

  const std::string* status = updatecheck_node.FindString("status");
  if (!status) {
    *error = "Missing status on updatecheck node";
    return false;
  }

  result->status = *status;
  if (result->status == "noupdate") {
    const auto* actions_node = updatecheck_node.Find("actions");
    if (actions_node) {
      ParseActions(*actions_node, result);
    }
    return true;
  }

  if (result->status == "ok") {
    const auto* actions_node = updatecheck_node.Find("actions");
    if (actions_node) {
      ParseActions(*actions_node, result);
    }

    const auto* urls_node = updatecheck_node.Find("urls");
    if (!urls_node) {
      *error = "Missing urls on updatecheck.";
      return false;
    }

    if (!ParseUrls(*urls_node, result, error)) {
      return false;
    }

    const auto* manifest_node = updatecheck_node.Find("manifest");
    if (!manifest_node) {
      *error = "Missing manifest on updatecheck.";
      return false;
    }
    return ParseManifest(*manifest_node, result, error);
  }

  // Return the |updatecheck| element status as a parsing error.
  *error = result->status;
  return false;
}

bool ParseApp(const base::Value& app_node_val,
              ProtocolParser::Result* result,
              std::string* error) {
  if (!app_node_val.is_dict()) {
    *error = "'app' is not a dictionary.";
    return false;
  }
  const base::Value::Dict& app_node = app_node_val.GetDict();
  for (const auto& cohort_key :
       {ProtocolParser::Result::kCohort, ProtocolParser::Result::kCohortHint,
        ProtocolParser::Result::kCohortName}) {
    const std::string* cohort_value = app_node.FindString(cohort_key);
    if (cohort_value) {
      result->cohort_attrs[cohort_key] = *cohort_value;
    }
  }
  const std::string* appid = app_node.FindString("appid");
  if (appid) {
    result->extension_id = *appid;
  }
  if (result->extension_id.empty()) {
    *error = "Missing appid on app node";
    return false;
  }

  // Read the |status| attribute for the app.
  // If the status is one of the defined app status error literals, then return
  // it in the result as if it were an updatecheck status, then stop parsing,
  // and return success.
  const std::string* status = app_node.FindString("status");
  if (status) {
    result->status = *status;
    if (result->status == "restricted" ||
        result->status == "error-unknownApplication" ||
        result->status == "error-invalidAppId" ||
        result->status == "error-osnotsupported" ||
        result->status == "error-hwnotsupported" ||
        result->status == "error-hash" ||
        result->status == "error-unsupportedprotocol" ||
        result->status == "error-internal") {
      return true;
    }

    // If the status was not handled above and the status is not "ok", then
    // this must be a status literal that that the parser does not know about.
    if (!result->status.empty() && result->status != "ok") {
      *error = "Unknown app status";
      return false;
    }
  }

  CHECK(result->status.empty() || result->status == "ok");

  if (const base::Value::List* data_node = app_node.FindList("data")) {
    base::ranges::for_each(*data_node, [&result](const base::Value& data) {
      ParseData(data, result);
    });
  }

  const auto* updatecheck_node = app_node.Find("updatecheck");
  if (!updatecheck_node) {
    *error = "Missing updatecheck on app.";
    return false;
  }

  return ParseUpdateCheck(*updatecheck_node, result, error);
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
  constexpr char kJSONPrefix[] = ")]}'";
  if (!base::StartsWith(response_json, kJSONPrefix,
                        base::CompareCase::SENSITIVE)) {
    ParseError("Missing secure JSON prefix.");
    return false;
  }
  const auto doc = base::JSONReader::Read(base::MakeStringPiece(
      response_json.begin() + std::char_traits<char>::length(kJSONPrefix),
      response_json.end()));
  if (!doc) {
    ParseError("JSON read error.");
    return false;
  }
  if (!doc->is_dict()) {
    ParseError("JSON document is not a dictionary.");
    return false;
  }
  const base::Value::Dict* response_node = doc->GetDict().FindDict("response");
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
    const std::optional<int> elapsed_seconds =
        daystart_node->FindInt("elapsed_seconds");
    if (elapsed_seconds) {
      results->daystart_elapsed_seconds = elapsed_seconds.value();
    }
    const std::optional<int> elapsed_days =
        daystart_node->FindInt("elapsed_days");
    if (elapsed_days) {
      results->daystart_elapsed_days = elapsed_days.value();
    }
  }

  const base::Value::Dict* systemrequirements_node =
      response_node->FindDict("systemrequirements");
  if (systemrequirements_node) {
    const std::string* platform =
        systemrequirements_node->FindString("platform");
    if (platform) {
      results->system_requirements.platform = *platform;
    }
    const std::string* arch = systemrequirements_node->FindString("arch");
    if (arch) {
      results->system_requirements.arch = *arch;
    }
    const std::string* min_os_version =
        systemrequirements_node->FindString("min_os_version");
    if (min_os_version) {
      results->system_requirements.min_os_version = *min_os_version;
    }
  }

  const base::Value::List* app_node = response_node->FindList("app");
  if (app_node) {
    for (const auto& app : *app_node) {
      Result result;
      std::string error;
      if (ParseApp(app, &result, &error)) {
        results->list.push_back(result);
      } else {
        ParseError("%s", error.c_str());
      }
    }
  }

  return true;
}

}  // namespace update_client
