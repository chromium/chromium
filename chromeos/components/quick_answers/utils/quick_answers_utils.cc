// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/quick_answers_utils.h"

#include <string>

#include "base/strings/stringprintf.h"

namespace chromeos {
namespace quick_answers {

namespace {

constexpr char kUnitConversionQueryRewriteTemplate[] = "Convert:%s";
constexpr char kDictionaryQueryRewriteTemplate[] = "Define:%s";
constexpr char kTranslationQueryRewriteTemplate[] = "Translate:%s";

}  // namespace

const PreprocessedOutput PreprocessRequest(const IntentInfo& intent_info) {
  PreprocessedOutput processed_output;
  processed_output.intent_info = intent_info;
  processed_output.query = intent_info.intent_text;

  switch (intent_info.intent_type) {
    case IntentType::kUnit:
      processed_output.query = base::StringPrintf(
          kUnitConversionQueryRewriteTemplate, intent_info.intent_text.c_str());
      break;
    case IntentType::kDictionary:
      processed_output.query = base::StringPrintf(
          kDictionaryQueryRewriteTemplate, intent_info.intent_text.c_str());
      break;
    case IntentType::kTranslation:
      processed_output.query = base::StringPrintf(
          kTranslationQueryRewriteTemplate, intent_info.intent_text.c_str());
      break;
    case IntentType::kUnknown:
      // TODO(llin): Update to NOTREACHED after integrating with TCLib.
      break;
  }
  return processed_output;
}

}  // namespace quick_answers
}  // namespace chromeos
