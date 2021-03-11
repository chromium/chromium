// Copyright (c) 2013 The Chromium Authors. All rights reserved.
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
  const base::DictionaryValue* images_value = NULL;
  if (theme_value->GetDictionary(keys::kThemeImages, &images_value)) {
    // Validate that the images are all strings.
    for (base::DictionaryValue::Iterator iter(*images_value); !iter.IsAtEnd();
         iter.Advance()) {
      // The value may be a dictionary of scales and files paths.
      // Or the value may be a file path, in which case a scale
      // of 100% is assumed.
      if (iter.value().is_dict()) {
        const base::DictionaryValue* inner_value = NULL;
        if (iter.value().GetAsDictionary(&inner_value)) {
          for (base::DictionaryValue::Iterator inner_iter(*inner_value);
               !inner_iter.IsAtEnd(); inner_iter.Advance()) {
            if (!inner_iter.value().is_string()) {
              *error = base::ASCIIToUTF16(errors::kInvalidThemeImages);
              return false;
            }
          }
        } else {
          *error = base::ASCIIToUTF16(errors::kInvalidThemeImages);
          return false;
        }
      } else if (!iter.value().is_string()) {
        *error = base::ASCIIToUTF16(errors::kInvalidThemeImages);
        return false;
      }
    }
    theme_info->theme_images_.reset(images_value->DeepCopy());
  }
  return true;
}

bool LoadColors(const base::DictionaryValue* theme_value,
                std::u16string* error,
                ThemeInfo* theme_info) {
  const base::DictionaryValue* colors_value = NULL;
  if (theme_value->GetDictionary(keys::kThemeColors, &colors_value)) {
    // Validate that the colors are RGB or RGBA lists.
    for (base::DictionaryValue::Iterator iter(*colors_value); !iter.IsAtEnd();
         iter.Advance()) {
      const base::ListValue* color_list = NULL;
      double alpha = 0.0;
      int color = 0;
      // The color must be a list...
      if (!iter.value().GetAsList(&color_list) ||
          // ... and either 3 items (RGB) or 4 (RGBA).
          ((color_list->GetSize() != 3) &&
           ((color_list->GetSize() != 4) ||
            // For RGBA, the fourth item must be a real or int alpha value.
            // Note that GetDouble() can get an integer value.
            !color_list->GetDouble(3, &alpha))) ||
          // For both RGB and RGBA, the first three items must be ints (R,G,B).
          !color_list->GetInteger(0, &color) ||
          !color_list->GetInteger(1, &color) ||
          !color_list->GetInteger(2, &color)) {
        *error = base::ASCIIToUTF16(errors::kInvalidThemeColors);
        return false;
      }
    }
    theme_info->theme_colors_.reset(colors_value->DeepCopy());
  }
  return true;
}

bool LoadTints(const base::DictionaryValue* theme_value,
               std::u16string* error,
               ThemeInfo* theme_info) {
  const base::DictionaryValue* tints_value = NULL;
  if (!theme_value->GetDictionary(keys::kThemeTints, &tints_value))
    return true;

  // Validate that the tints are all reals.
  for (base::DictionaryValue::Iterator iter(*tints_value); !iter.IsAtEnd();
       iter.Advance()) {
    const base::ListValue* tint_list = NULL;
    double v = 0.0;
    if (!iter.value().GetAsList(&tint_list) ||
        tint_list->GetSize() != 3 ||
        !tint_list->GetDouble(0, &v) ||
        !tint_list->GetDouble(1, &v) ||
        !tint_list->GetDouble(2, &v)) {
      *error = base::ASCIIToUTF16(errors::kInvalidThemeTints);
      return false;
    }
  }
  theme_info->theme_tints_.reset(tints_value->DeepCopy());
  return true;
}

bool LoadDisplayProperties(const base::DictionaryValue* theme_value,
                           std::u16string* error,
                           ThemeInfo* theme_info) {
  const base::DictionaryValue* display_properties_value = NULL;
  if (theme_value->GetDictionary(keys::kThemeDisplayProperties,
                                 &display_properties_value)) {
    theme_info->theme_display_properties_.reset(
        display_properties_value->DeepCopy());
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
  return theme_info ? theme_info->theme_images_.get() : NULL;
}

// static
const base::DictionaryValue* ThemeInfo::GetColors(const Extension* extension) {
  const ThemeInfo* theme_info = GetInfo(extension);
  return theme_info ? theme_info->theme_colors_.get() : NULL;
}

// static
const base::DictionaryValue* ThemeInfo::GetTints(const Extension* extension) {
  const ThemeInfo* theme_info = GetInfo(extension);
  return theme_info ? theme_info->theme_tints_.get() : NULL;
}

// static
const base::DictionaryValue* ThemeInfo::GetDisplayProperties(
    const Extension* extension) {
  const ThemeInfo* theme_info = GetInfo(extension);
  return theme_info ? theme_info->theme_display_properties_.get() : NULL;
}

ThemeHandler::ThemeHandler() {
}

ThemeHandler::~ThemeHandler() {
}

bool ThemeHandler::Parse(Extension* extension, std::u16string* error) {
  const base::DictionaryValue* theme_value = NULL;
  if (!extension->manifest()->GetDictionary(keys::kTheme, &theme_value)) {
    *error = base::ASCIIToUTF16(errors::kInvalidTheme);
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
      for (base::DictionaryValue::Iterator iter(*images_value); !iter.IsAtEnd();
           iter.Advance()) {
        std::string val;
        if (iter.value().GetAsString(&val)) {
          base::FilePath image_path = extension->path().Append(
              base::FilePath::FromUTF8Unsafe(val));
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
