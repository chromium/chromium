// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/protocol_parser_json.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "base/version.h"
#include "components/update_client/protocol_definition.h"

namespace update_client {

namespace {

bool ParseManifest(const base::Value& manifest_node,
                   ProtocolParser::Result* result,
                   std::string* error) {
  if (!manifest_node.is_dict()) {
    *error = "'manifest' is not a dictionary.";
  }
  const auto* version = manifest_node.FindKey("version");
  if (!version || !version->is_string()) {
    *error = "Missing version for manifest.";
    return false;
  }

  result->manifest.version = version->GetString();
  if (!base::Version(result->manifest.version).IsValid()) {
    *error =
        base::StrCat({"Invalid version: '", result->manifest.version, "'."});
    return false;
  }

  // Get the optional minimum browser version.
  const auto* browser_min_version = manifest_node.FindKey("prodversionmin");
  if (browser_min_version && browser_min_version->is_string()) {
    result->manifest.browser_min_version = browser_min_version->GetString();
    if (!base::Version(result->manifest.browser_min_version).IsValid()) {
      *error = base::StrCat({"Invalid prodversionmin: '",
                             result->manifest.browser_min_version, "'."});
      return false;
    }
  }

  const auto* packages_node = manifest_node.FindKey("packages");
  if (!packages_node || !packages_node->is_dict()) {
    *error = "Missing packages in manifest or 'packages' is not a dictionary.";
    return false;
  }
  const auto* package_node = packages_node->FindKey("package");
  if (!package_node || !package_node->is_list()) {
    *error = "Missing package in packages.";
    return false;
  }

  for (const auto& package : package_node->GetList()) {
    if (!package.is_dict()) {
      *error = "'package' is not a dictionary.";
      return false;
    }
    ProtocolParser::Result::Manifest::Package p;
    const auto* name = package.FindKey("name");
    if (!name || !name->is_string()) {
      *error = "Missing name for package.";
      return false;
    }
    p.name = name->GetString();

    const auto* namediff = package.FindKey("namediff");
    if (namediff && namediff->is_string())
      p.namediff = namediff->GetString();

    const auto* fingerprint = package.FindKey("fp");
    if (fingerprint && fingerprint->is_string())
      p.fingerprint = fingerprint->GetString();

    const auto* hash_sha256 = package.FindKey("hash_sha256");
    if (hash_sha256 && hash_sha256->is_string())
      p.hash_sha256 = hash_sha256->GetString();

    const auto* size = package.FindKey("size");
    if (size && (size->is_int() || size->is_double())) {
      const auto val = size->GetDouble();
      if (0 <= val && val < kProtocolMaxInt)
        p.size = size->GetDouble();
    }

    const auto* hashdiff_sha256 = package.FindKey("hashdiff_sha256");
    if (hashdiff_sha256 && hashdiff_sha256->is_string())
      p.hashdiff_sha256 = hashdiff_sha256->GetString();

    const auto* sizediff = package.FindKey("sizediff");
    if (sizediff && (sizediff->is_int() || sizediff->is_double())) {
      const auto val = sizediff->GetDouble();
      if (0 <= val && val < kProtocolMaxInt)
        p.sizediff = sizediff->GetDouble();
    }

    result->manifest.packages.push_back(std::move(p));
  }

  return true;
}

void ParseActions(const base::Value& actions_node,
                  ProtocolParser::Result* result) {
  if (!actions_node.is_dict())
    return;

  const auto* action_node = actions_node.FindKey("action");
  if (!action_node || !action_node->is_list())
    return;

  const auto& action_list = action_node->GetList();
  if (action_list.empty() || !action_list[0].is_dict())
    return;

  const auto* run = action_list[0].FindKey("run");
  if (run && run->is_string())
    result->action_run = run->GetString();
}

bool ParseUrls(const base::Value& urls_node,
               ProtocolParser::Result* result,
               std::string* error) {
  if (!urls_node.is_dict()) {
    *error = "'urls' is not a dictionary.";
    return false;
  }
  const auto* url_node = urls_node.FindKey("url");
  if (!url_node || !url_node->is_list()) {
    *error = "Missing url on urls.";
    return false;
  }

  for (const auto& url : url_node->GetList()) {
    if (!url.is_dict())
      continue;
    const auto* codebase = url.FindKey("codebase");
    if (codebase && codebase->is_string()) {
      GURL crx_url(codebase->GetString());
      if (crx_url.is_valid())
        result->crx_urls.push_back(std::move(crx_url));
    }
    const auto* codebasediff = url.FindKey("codebasediff");
    if (codebasediff && codebasediff->is_string()) {
      GURL crx_diffurl(codebasediff->GetString());
      if (crx_diffurl.is_valid())
        result->crx_diffurls.push_back(std::move(crx_diffurl));
    }
  }

  // Expect at least one url for full update.
  if (result->crx_urls.empty()) {
    *error = "Missing valid url for full update.";
    return false;
  }

  return true;
}

bool ParseUpdateCheck(const base::Value& updatecheck_node,
                      ProtocolParser::Result* result,
                      std::string* error) {
  if (!updatecheck_node.is_dict()) {
    *error = "'updatecheck' is not a dictionary.";
    return false;
  }
  const auto* status = updatecheck_node.FindKey("status");
  if (!status || !status->is_string()) {
    *error = "Missing status on updatecheck node";
    return false;
  }

  result->status = status->GetString();
  if (result->status == "noupdate") {
    const auto* actions_node = updatecheck_node.FindKey("actions");
    if (actions_node)
      ParseActions(*actions_node, result);
    return true;
  }

  if (result->status == "ok") {
    const auto* actions_node = updatecheck_node.FindKey("actions");
    if (actions_node)
      ParseActions(*actions_node, result);

    const auto* urls_node = updatecheck_node.FindKey("urls");
    if (!urls_node) {
      *error = "Missing urls on updatecheck.";
      return false;
    }

    if (!ParseUrls(*urls_node, result, error))
      return false;

    const auto* manifest_node = updatecheck_node.FindKey("manifest");
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

bool ParseApp(const base::Value& app_node,
              ProtocolParser::Result* result,
              std::string* error) {
  if (!app_node.is_dict()) {
    *error = "'app' is not a dictionary.";
    return false;
  }
  for (const auto* cohort_key :
       {ProtocolParser::Result::kCohort, ProtocolParser::Result::kCohortHint,
        ProtocolParser::Result::kCohortName}) {
    const auto* cohort_value = app_node.FindKey(cohort_key);
    if (cohort_value && cohort_value->is_string())
      result->cohort_attrs[cohort_key] = cohort_value->GetString();
  }
  const auto* appid = app_node.FindKey("appid");
  if (appid && appid->is_string())
    result->extension_id = appid->GetString();
  if (result->extension_id.empty()) {
    *error = "Missing appid on app node";
    return false;
  }

  // Read the |status| attribute for the app.
  // If the status is one of the defined app status error literals, then return
  // it in the result as if it were an updatecheck status, then stop parsing,
  // and return success.
  const auto* status = app_node.FindKey("status");
  if (status && status->is_string()) {
    result->status = status->GetString();
    if (result->status == "restricted" ||
        result->status == "error-unknownApplication" ||
        result->status == "error-invalidAppId")
      return true;

    // If the status was not handled above and the status is not "ok", then
    // this must be a status literal that that the parser does not know about.
    if (!result->status.empty() && result->status != "ok") {
      *error = "Unknown app status";
      return false;
    }
  }

  DCHECK(result->status.empty() || result->status == "ok");
  const auto* updatecheck_node = app_node.FindKey("updatecheck");
  if (!updatecheck_node) {
    *error = "Missing updatecheck on app.";
    return false;
  }

  return ParseUpdateCheck(*updatecheck_node, result, error);
}

}  // namespace

bool ProtocolParserJSON::DoParse(const std::string& response_json,
                                 Results* results) {
  DCHECK(results);

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
  const auto doc = base::JSONReader::Read(
      {response_json.begin() + std::char_traits<char>::length(kJSONPrefix),
       response_json.end()});
  if (!doc) {
    ParseError("JSON read error.");
    return false;
  }
  if (!doc->is_dict()) {
    ParseError("JSON document is not a dictionary.");
    return false;
  }
  const auto* response_node = doc->FindKey("response");
  if (!response_node || !response_node->is_dict()) {
    ParseError("Missing 'response' element or 'response' is not a dictionary.");
    return false;
  }
  const auto* protocol = response_node->FindKey("protocol");
  if (!protocol || !protocol->is_string()) {
    ParseError("Missing/non-string protocol.");
    return false;
  }
  if (protocol->GetString() != kProtocolVersion) {
    ParseError("Incorrect protocol. (expected '%s', found '%s')",
               kProtocolVersion, protocol->GetString().c_str());
    return false;
  }

  const auto* daystart_node = response_node->FindKey("daystart");
  if (daystart_node && daystart_node->is_dict()) {
    const auto* elapsed_seconds = daystart_node->FindKey("elapsed_seconds");
    if (elapsed_seconds && elapsed_seconds->is_int())
      results->daystart_elapsed_seconds = elapsed_seconds->GetInt();
    const auto* elapsed_days = daystart_node->FindKey("elapsed_days");
    if (elapsed_days && elapsed_days->is_int())
      results->daystart_elapsed_days = elapsed_days->GetInt();
  }

  const auto* app_node = response_node->FindKey("app");
  if (app_node && app_node->is_list()) {
    for (const auto& app : app_node->GetList()) {
      Result result;
      std::string error;
      if (ParseApp(app, &result, &error))
        results->list.push_back(result);
      else
        ParseError("%s", error.c_str());
    }
  }

  return true;
}

}  // namespace update_client
