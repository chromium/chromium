// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/input_ime/input_components_handler.h"

#include <stddef.h>

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

InputComponentInfo::InputComponentInfo() = default;

InputComponentInfo::InputComponentInfo(const InputComponentInfo& other) =
    default;

InputComponentInfo::~InputComponentInfo() {}

InputComponents::InputComponents() {}
InputComponents::~InputComponents() {}

// static
const std::vector<InputComponentInfo>* InputComponents::GetInputComponents(
    const Extension* extension) {
  InputComponents* info = static_cast<InputComponents*>(
      extension->GetManifestData(keys::kInputComponents));
  return info ? &info->input_components : NULL;
}

InputComponentsHandler::InputComponentsHandler() {
}

InputComponentsHandler::~InputComponentsHandler() {
}

bool InputComponentsHandler::Parse(Extension* extension,
                                   std::u16string* error) {
  std::unique_ptr<InputComponents> info(new InputComponents);
  const base::ListValue* list_value = NULL;
  if (!extension->manifest()->GetList(keys::kInputComponents, &list_value)) {
    *error = base::ASCIIToUTF16(errors::kInvalidInputComponents);
    return false;
  }
  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    const base::DictionaryValue* module_value = NULL;
    std::string name_str;
    std::string id_str;
    std::set<std::string> languages;
    std::set<std::string> layouts;
    GURL input_view_url;
    GURL options_page_url;

    if (!list_value->GetDictionary(i, &module_value)) {
      *error = base::ASCIIToUTF16(errors::kInvalidInputComponents);
      return false;
    }

    // Get input_components[i].name.
    if (!module_value->GetString(keys::kName, &name_str)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidInputComponentName, base::NumberToString(i));
      return false;
    }

    // Get input_components[i].id.
    if (!module_value->GetString(keys::kId, &id_str)) {
      id_str = "";
    }

    // Get input_components[i].language.
    // Both string and list of string are allowed to be compatibile with old
    // input_ime manifest specification.
    const base::Value* language_value = NULL;
    if (module_value->Get(keys::kLanguage, &language_value)) {
      if (language_value->is_string()) {
        std::string language_str;
        language_value->GetAsString(&language_str);
        languages.insert(language_str);
      } else if (language_value->is_list()) {
        const base::ListValue* language_list = NULL;
        language_value->GetAsList(&language_list);
        for (size_t j = 0; j < language_list->GetSize(); ++j) {
          std::string language_str;
          if (language_list->GetString(j, &language_str))
            languages.insert(language_str);
        }
      }
    }

    // Get input_components[i].layouts.
    const base::ListValue* layouts_value = NULL;
    if (module_value->GetList(keys::kLayouts, &layouts_value)) {
      for (size_t j = 0; j < layouts_value->GetSize(); ++j) {
        std::string layout_name_str;
        if (!layouts_value->GetString(j, &layout_name_str)) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidInputComponentLayoutName, base::NumberToString(i),
              base::NumberToString(j));
          return false;
        }
        layouts.insert(layout_name_str);
      }
    }

    // Get input_components[i].input_view_url.
    // Note: 'input_view' is optional in manifest.
    std::string input_view_str;
    if (module_value->GetString(keys::kInputView, &input_view_str)) {
      input_view_url = extension->GetResourceURL(input_view_str);
      if (!input_view_url.is_valid()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidInputView,
                                                     base::NumberToString(i));
        return false;
      }
    }

    // Get input_components[i].options_page_url.
    // Note: 'options_page' is optional in manifest.
    std::string options_page_str;
    if (module_value->GetString(keys::kImeOptionsPage, &options_page_str)) {
      options_page_url = extension->GetResourceURL(options_page_str);
      if (!options_page_url.is_valid()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidOptionsPage, base::NumberToString(i));
        return false;
      }
    } else {
      // Fall back to extension's options page for backward compatibility.
      options_page_url = extensions::OptionsPageInfo::GetOptionsPage(extension);
    }

    info->input_components.push_back(InputComponentInfo());
    info->input_components.back().name = name_str;
    info->input_components.back().id = id_str;
    info->input_components.back().languages = languages;
    info->input_components.back().layouts.insert(layouts.begin(),
        layouts.end());
    info->input_components.back().options_page_url = options_page_url;
    info->input_components.back().input_view_url = input_view_url;
  }
  extension->SetManifestData(keys::kInputComponents, std::move(info));
  return true;
}

const std::vector<std::string>
InputComponentsHandler::PrerequisiteKeys() const {
  return SingleKey(keys::kOptionsPage);
}

base::span<const char* const> InputComponentsHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kInputComponents};
  return kKeys;
}

}  // namespace extensions
