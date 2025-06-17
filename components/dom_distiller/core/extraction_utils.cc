// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/extraction_utils.h"

#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "components/grit/components_resources.h"
#include "third_party/dom_distiller_js/dom_distiller.pb.h"
#include "third_party/dom_distiller_js/dom_distiller_json_converter.h"
#include "ui/base/resource/resource_bundle.h"

namespace dom_distiller {
namespace {
const char* kOptionsPlaceholder = "$$OPTIONS";
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
  size_t options_offset = script.find(kOptionsPlaceholder);
  CHECK_NE(std::string::npos, options_offset);
  CHECK_EQ(std::string::npos,
           script.find(kOptionsPlaceholder, options_offset + 1));

  return script.replace(options_offset, strlen(kOptionsPlaceholder),
                        options_json);
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
  return script;
}

}  // namespace dom_distiller
