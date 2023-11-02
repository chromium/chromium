// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/theme_handler.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

bool LoadImages(const base::DictionaryValue* theme_value,
                std::u16string* error,
                ThemeInfo* theme_info) {
  const base::DictionaryValue* images_value = nullptr;
  if (theme_value->GetDictionary(keys::kThemeImages, &images_value)) {
    // Validate that the images are all strings.
    for (const auto item : images_value->GetDict()) {
      // The value may be a dictionary of scales and files paths.
      // Or the value may be a file path, in which case a scale
      // of 100% is assumed.
      if (item.second.is_dict()) {
        for (const auto inner_item : item.second.GetDict()) {
          if (!inner_item.second.is_string()) {
            *error = errors::kInvalidThemeImages;
            return false;
          }
        }
      } else if (!item.second.is_string()) {
        *error = errors::kInvalidThemeImages;
        return false;
      }
    }
    theme_info->theme_images_ = base::DictionaryValue::From(
        base::Value::ToUniquePtrValue(images_value->Clone()));
  }
  return true;
}

bool LoadColors(const base::Value* theme_value,
                std::u16string* error,
                ThemeInfo* theme_info) {
  DCHECK(theme_value);
  DCHECK(theme_value->is_dict());
  const base::Value* colors_value =
      theme_value->FindDictPath(keys::kThemeColors);
  if (colors_value) {
    // Validate that the colors are RGB or RGBA lists.
    for (const auto it : colors_value->DictItems()) {
      if (!it.second.is_list()) {
        *error = errors::kInvalidThemeColors;
        return false;
      }
      const base::Value::List& color_list = it.second.GetList();

      // There must be either 3 items (RGB), or 4 (RGBA).
      if (!(color_list.size() == 3 || color_list.size() == 4)) {
        *error = errors::kInvalidThemeColors;
        return false;
      }

      // The first three items (RGB), must be ints:
      if (!(color_list[0].is_int() && color_list[1].is_int() &&
            color_list[2].is_int())) {
        *error = errors::kInvalidThemeColors;
        return false;
      }

      // If there is a 4th item (alpha), it may be either int or double:
      if (color_list.size() == 4 &&
          !(color_list[3].is_int() || color_list[3].is_double())) {
        *error = errors::kInvalidThemeColors;
        return false;
      }
    }

    theme_info->theme_colors_ =
        base::Value::ToUniquePtrValue(colors_value->Clone());
  }
  return true;
}

bool LoadTints(const base::DictionaryValue* theme_value,
               std::u16string* error,
               ThemeInfo* theme_info) {
  const base::DictionaryValue* tints_value = nullptr;
  if (!theme_value->GetDictionary(keys::kThemeTints, &tints_value))
    return true;

  // Validate that the tints are all reals.
  for (const auto item : tints_value->GetDict()) {
    if (!item.second.is_list()) {
      *error = errors::kInvalidThemeTints;
      return false;
    }

    const base::Value::List& tint_list = item.second.GetList();
    if (tint_list.size() != 3) {
      *error = errors::kInvalidThemeTints;
      return false;
    }

    if (!tint_list[0].GetIfDouble() || !tint_list[1].GetIfDouble() ||
        !tint_list[2].GetIfDouble()) {
      *error = errors::kInvalidThemeTints;
      return false;
    }
  }

  theme_info->theme_tints_ = base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(tints_value->Clone()));
  return true;
}

bool LoadDisplayProperties(const base::DictionaryValue* theme_value,
                           std::u16string* error,
                           ThemeInfo* theme_info) {
  const base::DictionaryValue* display_properties_value = nullptr;
  if (theme_value->GetDictionary(keys::kThemeDisplayProperties,
                                 &display_properties_value)) {
    theme_info->theme_display_properties_ = base::DictionaryValue::From(
        base::Value::ToUniquePtrValue(display_properties_value->Clone()));
  }
  return true;
}

const ThemeInfo* GetInfo(const Extension* extension) {
  return static_cast<ThemeInfo*>(extension->GetManifestData(keys::kTheme));
}

}  // namespace

ThemeInfo::ThemeInfo() {
}

ThemeInfo::~ThemeInfo() {
}

// static
const base::DictionaryValue* ThemeInfo::GetImages(const Extension* extension) {
  const ThemeInfo* theme_info = GetInfo(extension);
  return theme_info ? theme_info->theme_images_.get() : nullptr;
}

// static
const base::Value* ThemeInfo::GetColors(const Extension* extension) {
  const ThemeInfo* theme_info = GetInfo(extension);
  return theme_info ? theme_info->theme_colors_.get() : nullptr;
}

// static
const base::DictionaryValue* ThemeInfo::GetTints(const Extension* extension) {
  const ThemeInfo* theme_info = GetInfo(extension);
  return theme_info ? theme_info->theme_tints_.get() : nullptr;
}

// static
const base::DictionaryValue* ThemeInfo::GetDisplayProperties(
    const Extension* extension) {
  const ThemeInfo* theme_info = GetInfo(extension);
  return theme_info ? theme_info->theme_display_properties_.get() : nullptr;
}

ThemeHandler::ThemeHandler() {
}

ThemeHandler::~ThemeHandler() {
}

bool ThemeHandler::Parse(Extension* extension, std::u16string* error) {
  const base::DictionaryValue* theme_value = nullptr;
  if (!extension->manifest()->GetDictionary(keys::kTheme, &theme_value)) {
    *error = errors::kInvalidTheme;
    return false;
  }

  std::unique_ptr<ThemeInfo> theme_info(new ThemeInfo);
  if (!LoadImages(theme_value, error, theme_info.get()))
    return false;
  if (!LoadColors(theme_value, error, theme_info.get()))
    return false;
  if (!LoadTints(theme_value, error, theme_info.get()))
    return false;
  if (!LoadDisplayProperties(theme_value, error, theme_info.get()))
    return false;

  extension->SetManifestData(keys::kTheme, std::move(theme_info));
  return true;
}

bool ThemeHandler::Validate(const Extension* extension,
                            std::string* error,
                            std::vector<InstallWarning>* warnings) const {
  // Validate that theme images exist.
  if (extension->is_theme()) {
    const base::DictionaryValue* images_value =
        extensions::ThemeInfo::GetImages(extension);
    if (images_value) {
      for (const auto item : images_value->GetDict()) {
        const std::string* val = item.second.GetIfString();
        if (val) {
          base::FilePath image_path =
              extension->path().Append(base::FilePath::FromUTF8Unsafe(*val));
          if (!base::PathExists(image_path)) {
            *error =
                l10n_util::GetStringFUTF8(IDS_EXTENSION_INVALID_IMAGE_PATH,
                                          image_path.LossyDisplayName());
            return false;
          }
        }
      }
    }
  }
  return true;
}

base::span<const char* const> ThemeHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kTheme};
  return kKeys;
}

}  // namespace extensions
