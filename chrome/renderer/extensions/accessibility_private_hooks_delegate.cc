// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/accessibility_private_hooks_delegate.h"

#include "base/i18n/unicodestring.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace extensions {

namespace {
constexpr char kGetDisplayLanguage[] =
    "accessibilityPrivate.getDisplayLanguage";
constexpr char kUndeterminedLanguage[] = "und";
}  // namespace

using RequestResult = APIBindingHooks::RequestResult;

AccessibilityPrivateHooksDelegate::AccessibilityPrivateHooksDelegate() =
    default;
AccessibilityPrivateHooksDelegate::~AccessibilityPrivateHooksDelegate() =
    default;

RequestResult AccessibilityPrivateHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    std::vector<v8::Local<v8::Value>>* arguments,
    const APITypeReferenceMap& refs) {
  // Error checks.
  // Ensure we would like to call the GetDisplayLanguage function.
  if (method_name != kGetDisplayLanguage)
    return RequestResult(RequestResult::NOT_HANDLED);
  // Ensure arguments are successfully parsed and converted.
  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(*parse_result.error);
    return result;
  }
  return HandleGetDisplayLanguage(GetScriptContextFromV8ContextChecked(context),
                                  *parse_result.arguments);
}

// Called to translate |language_code_to_translate| into human-readable string
// in the language specified by |target_language_code|. For example, if
// language_code_to_translate = 'en' and target_language_code = 'fr', then this
// function returns 'anglais'.
// If language_code_to_translate = 'fr' and target_language_code = 'en', then
// this function returns 'french'.
RequestResult AccessibilityPrivateHooksDelegate::HandleGetDisplayLanguage(
    ScriptContext* script_context,
    const std::vector<v8::Local<v8::Value>>& parsed_arguments) {
  DCHECK(script_context->extension());
  DCHECK_EQ(2u, parsed_arguments.size());
  DCHECK(parsed_arguments[0]->IsString());
  DCHECK(parsed_arguments[1]->IsString());

  std::string language_code_to_translate =
      gin::V8ToString(script_context->isolate(), parsed_arguments[0]);
  std::string target_language_code =
      gin::V8ToString(script_context->isolate(), parsed_arguments[1]);
  // The locale whose language code we want to translate.
  icu::Locale locale_to_translate =
      icu::Locale(language_code_to_translate.c_str());
  // The locale that |display_language| should be in.
  icu::Locale target_locale = icu::Locale(target_language_code.c_str());

  // Validate locales.
  // Get list of available locales. Please see the ICU User Guide for more
  // details: http://userguide.icu-project.org/locale.
  bool valid_arg1 = false;
  bool valid_arg2 = false;
  int32_t num_locales = 0;
  const icu::Locale* available_locales =
      icu::Locale::getAvailableLocales(num_locales);
  for (int32_t i = 0; i < num_locales; ++i) {
    // Check both the language and country for each locale.
    const char* current_language = available_locales[i].getLanguage();
    const char* current_country = available_locales[i].getCountry();
    if (strcmp(locale_to_translate.getLanguage(), current_language) == 0 &&
        strcmp(locale_to_translate.getCountry(), current_country) == 0) {
      valid_arg1 = true;
    }
    if (strcmp(target_locale.getLanguage(), current_language) == 0 &&
        strcmp(target_locale.getCountry(), current_country) == 0) {
      valid_arg2 = true;
    }
  }

  // If either of the language codes is invalid, we should return empty string.
  if (!(valid_arg1 && valid_arg2)) {
    RequestResult empty_result(RequestResult::HANDLED);
    empty_result.return_value = gin::StringToV8(script_context->isolate(), "");
    return empty_result;
  }

  icu::UnicodeString display_language;
  locale_to_translate.getDisplayLanguage(target_locale, display_language);
  std::string language_result =
      base::UTF16ToUTF8(base::i18n::UnicodeStringToString16(display_language));
  // Instead of returning "und", which is what the ICU Locale class returns for
  // undetermined languages, we would simply like to return an empty string
  // to communicate that we could not determine the display language.
  if (language_result == kUndeterminedLanguage)
    language_result = "";
  RequestResult result(RequestResult::HANDLED);
  result.return_value =
      gin::StringToV8(script_context->isolate(), language_result);
  return result;
}

}  // namespace extensions
