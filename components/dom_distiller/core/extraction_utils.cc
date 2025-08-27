// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/extraction_utils.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/grit/components_resources.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "third_party/dom_distiller_js/dom_distiller_json_converter.h"
#include "ui/base/resource/resource_bundle.h"

namespace dom_distiller {
namespace {
const char* kOptionsPlaceholder = "$$OPTIONS";
const char* kMinScorePlaceholder = "$$MIN_SCORE_PLACEHOLDER";
const char* kMinContentLengthPlaceholder = "$$MIN_CONTENT_LENGTH_PLACEHOLDER";

void ReplaceScriptPlaceholder(std::string& script,
                              const char* placeholder,
                              std::string& replacement) {
  size_t offset = script.find(placeholder);
  CHECK_NE(std::string::npos, offset);
  CHECK_EQ(std::string::npos, script.find(placeholder, offset + 1));
  script.replace(offset, strlen(placeholder), replacement);
}
}  // namespace

std::string GetDistillerScriptWithOptions(
    const dom_distiller::proto::DomDistillerOptions& options) {
  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_DISTILLER_JS);
  CHECK(!script.empty());

  base::Value options_value =
      dom_distiller::proto::json::DomDistillerOptions::WriteToValue(options);
  std::string options_json;
  if (!base::JSONWriter::Write(options_value, &options_json)) {
    NOTREACHED();
  }
  ReplaceScriptPlaceholder(script, kOptionsPlaceholder, options_json);
  return script;
}

std::string GetReadabilityDistillerScript() {
  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_READABILITY_DISTILLER_JS);
  CHECK(!script.empty());
  return script;
}

std::string GetReadabilityTriggeringScript() {
  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_READABILITY_TRIGGERING_JS);
  CHECK(!script.empty());

  std::string min_score =
      base::NumberToString(GetReadabilityHeuristicMinScore());
  std::string min_content_length =
      base::NumberToString(GetReadabilityHeuristicMinContentLength());
  ReplaceScriptPlaceholder(script, kMinScorePlaceholder, min_score);
  ReplaceScriptPlaceholder(script, kMinContentLengthPlaceholder,
                           min_content_length);

  return script;
}

}  // namespace dom_distiller
