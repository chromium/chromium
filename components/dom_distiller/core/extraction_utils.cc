// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/extraction_utils.h"

#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/dom_distiller/core/distiller_options.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/grit/components_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace dom_distiller {
namespace {
const char* kMinScorePlaceholder = "$$MIN_SCORE_PLACEHOLDER";
const char* kMinContentLengthPlaceholder = "$$MIN_CONTENT_LENGTH_PLACEHOLDER";
const char* kAllowedVideoRegexPlaceholder = "$$ALLOWED_VIDEO_REGEX";

void ReplaceScriptPlaceholder(std::string& script,
                              const char* placeholder,
                              std::string& replacement) {
  size_t offset = script.find(placeholder);
  CHECK_NE(std::string::npos, offset);
  CHECK_EQ(std::string::npos, script.find(placeholder, offset + 1));
  script.replace(offset, strlen(placeholder), replacement);
}
}  // namespace

std::string GetDistillerScript(const DistillerOptions& options) {
  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_WEB_DISTILLER_JS);
  CHECK(!script.empty());

  std::string allowed_regex_json;
  if (options.allowed_video_regex.has_value()) {
    if (!base::JSONWriter::Write(base::Value(*options.allowed_video_regex),
                                 &allowed_regex_json)) {
      NOTREACHED();
    }
  } else {
    allowed_regex_json = "undefined";
  }
  ReplaceScriptPlaceholder(script, kAllowedVideoRegexPlaceholder,
                           allowed_regex_json);
  return script;
}

std::string GetDistillerTriggeringScript() {
  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_WEB_DISTILLER_TRIGGERING_JS);
  CHECK(!script.empty());

  std::string min_score = base::NumberToString(kHeuristicMinScore);
  std::string min_content_length =
      base::NumberToString(kHeuristicMinContentLength);
  ReplaceScriptPlaceholder(script, kMinScorePlaceholder, min_score);
  ReplaceScriptPlaceholder(script, kMinContentLengthPlaceholder,
                           min_content_length);

  return script;
}

int CountWords(const std::string& text_content) {
  int result = 0;
  bool prev_char_whitespace = false;
  for (const char& it : text_content) {
    bool cur_char_whitespace = base::IsAsciiWhitespace(it);
    if (prev_char_whitespace && !cur_char_whitespace) {
      result++;
    }
    prev_char_whitespace = cur_char_whitespace;
  }

  return result + 1;
}

bool ParseDistillationResult(const base::Value& value,
                             DistilledPageProto* result) {
  if (!value.is_dict()) {
    return false;
  }

  const base::DictValue* dict_value = value.GetIfDict();

  if (const std::string* title = dict_value->FindString(kDistilledTitleKey)) {
    result->set_title(*title);
  }
  if (const std::string* content =
          dict_value->FindString(kDistilledContentKey)) {
    result->set_html(*content);
  }

  if (const std::string* dir =
          dict_value->FindString(kDistilledTextDirectionKey);
      dir && !dir->empty()) {
    result->set_text_direction(*dir);
  } else {
    result->set_text_direction("auto");
  }

  if (const std::string* text_content =
          dict_value->FindString(kDistilledTextContentKey)) {
    result->set_text_content(*text_content);
    result->set_word_count(CountWords(*text_content));
  }

  return true;
}

}  // namespace dom_distiller
