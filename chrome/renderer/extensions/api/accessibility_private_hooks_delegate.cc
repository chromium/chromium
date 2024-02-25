// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/accessibility_private_hooks_delegate.h"

#include "base/i18n/unicodestring.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/extension.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {
constexpr char kGetDisplayNameForLocale[] =
    "accessibilityPrivate.getDisplayNameForLocale";
constexpr char kUndeterminedLocale[] = "und";
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
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  // Error checks.
  // Ensure we would like to call the GetDisplayNameForLocale function.
  if (method_name != kGetDisplayNameForLocale)
    return RequestResult(RequestResult::NOT_HANDLED);
  // Ensure arguments are successfully parsed and converted.
  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(*parse_result.error);
    return result;
  }
  return HandleGetDisplayNameForLocale(
      GetScriptContextFromV8ContextChecked(context), *parse_result.arguments);
}

RequestResult AccessibilityPrivateHooksDelegate::HandleGetDisplayNameForLocale(
    ScriptContext* script_context,
    const v8::LocalVector<v8::Value>& parsed_arguments) {
  DCHECK(script_context->extension());
  DCHECK_EQ(2u, parsed_arguments.size());
  DCHECK(parsed_arguments[0]->IsString());
  DCHECK(parsed_arguments[1]->IsString());

  const std::string locale =
      gin::V8ToString(script_context->isolate(), parsed_arguments[0]);
  const std::string display_locale =
      gin::V8ToString(script_context->isolate(), parsed_arguments[1]);

  bool found_valid_result = false;
  std::string locale_result;
  if (l10n_util::IsValidLocaleSyntax(locale) &&
      l10n_util::IsValidLocaleSyntax(display_locale)) {
    locale_result = base::UTF16ToUTF8(l10n_util::GetDisplayNameForLocale(
        locale, display_locale, true /* is_ui */));
    // Check for valid locales before getting the display name.
    // The ICU Locale class returns "und" for undetermined locales, and
    // returns the locale string directly if it has no translation.
    // Treat these cases as invalid results.
    found_valid_result =
        locale_result != kUndeterminedLocale && locale_result != locale;
  }

  // We return an empty string to communicate that we could not determine
  // the display locale.
  if (!found_valid_result)
    locale_result = std::string();

  RequestResult result(RequestResult::HANDLED);
  result.return_value =
      gin::StringToV8(script_context->isolate(), locale_result);
  return result;
}

}  // namespace extensions
