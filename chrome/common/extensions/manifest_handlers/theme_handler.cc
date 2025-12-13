// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/theme_handler.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handler_helpers.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

bool IsThemeImageMimeTypeValid(const base::FilePath& relative_path,
                               std::vector<std::string>* warnings) {
  // In case of an image with no file extension, issue a warning and allow it
  // for compatibility with existing themes.
  if (relative_path.Extension().empty()) {
    warnings->emplace_back(ErrorUtils::FormatErrorMessage(
        errors::kThemeImageMissingFileExtension, relative_path.AsUTF8Unsafe()));
    return true;
  }

  if (!manifest_handler_helpers::IsSupportedExtensionImageMimeType(
          relative_path)) {
    // Issue a warning and ignore this entry. This is a warning and not a
    // hard-error to preserve both backwards compatibility and potential
    // future-compatibility if mime types change.
    warnings->emplace_back(ErrorUtils::FormatErrorMessage(
        errors::kInvalidThemeImageMimeType, relative_path.AsUTF8Unsafe()));
    return false;
  }

  return true;
}

bool LoadImages(const Extension& extension,
                const base::Value::Dict& theme_dict,
                std::u16string* error,
                std::vector<std::string>* warnings,
                ThemeInfo* theme_info) {
  if (const base::Value::Dict* images_dict =
          theme_dict.FindDict(keys::kThemeImages)) {
    ThemeInfo::ThemeImages theme_images;

    // Validate that the images are all strings.
    for (const auto [key, value] : *images_dict) {
      std::vector<ThemeInfo::ThemeResource> theme_resources;

      // The value may be a dictionary of scales and files paths.
      // Or the value may be a file path, in which case a scale
      // of 100% is assumed.
      if (value.is_dict()) {
        for (const auto [inner_key, inner_value] : value.GetDict()) {
          if (!inner_value.is_string()) {
            *error = errors::kInvalidThemeImagesValueType;
            return false;
          }

          ExtensionResource local_path =
              extension.GetResource(inner_value.GetString());
          if (local_path.empty()) {
            *error = errors::kInvalidThemeImagesPath;
            return false;
          }

          if (!IsThemeImageMimeTypeValid(local_path.relative_path(),
                                         warnings)) {
            theme_resources.clear();
            break;
          }

          theme_resources.emplace_back(std::move(local_path), inner_key);
        }
      } else if (value.is_string()) {
        ExtensionResource local_path = extension.GetResource(value.GetString());
        if (local_path.empty()) {
          *error = errors::kInvalidThemeImagesPath;
          return false;
        }

        if (IsThemeImageMimeTypeValid(local_path.relative_path(), warnings)) {
          theme_resources.emplace_back(std::move(local_path), std::string());
        }
      } else {
        *error = errors::kInvalidThemeImagesValueType;
        return false;
      }

      if (!theme_resources.empty()) {
        theme_images[key] = std::move(theme_resources);
      }
    }
    theme_info->theme_images_ = std::move(theme_images);
  }
  return true;
}

bool LoadColors(const base::Value::Dict& theme_dict,
                std::u16string* error,
                ThemeInfo* theme_info) {
  if (const base::Value::Dict* colors_value =
          theme_dict.FindDict(keys::kThemeColors)) {
    // Validate that the colors are RGB or RGBA lists.
    for (const auto [key, value] : *colors_value) {
      if (!value.is_list()) {
        *error = errors::kInvalidThemeColors;
        return false;
      }
      const base::Value::List& color_list = value.GetList();

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

    theme_info->theme_colors_ = colors_value->Clone();
  }
  return true;
}

bool LoadTints(const base::Value::Dict& theme_dict,
               std::u16string* error,
               ThemeInfo* theme_info) {
  const base::Value::Dict* tints_dict = theme_dict.FindDict(keys::kThemeTints);
  if (!tints_dict) {
    return true;
  }

  // Validate that the tints are all reals.
  for (const auto [key, value] : *tints_dict) {
    if (!value.is_list()) {
      *error = errors::kInvalidThemeTints;
      return false;
    }

    const base::Value::List& tint_list = value.GetList();
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

  theme_info->theme_tints_ = tints_dict->Clone();
  return true;
}

bool LoadDisplayProperties(const base::Value::Dict& theme_dict,
                           std::u16string* error,
                           ThemeInfo* theme_info) {
  if (const base::Value::Dict* display_properties_value =
          theme_dict.FindDict(keys::kThemeDisplayProperties)) {
    theme_info->theme_display_properties_ = display_properties_value->Clone();
  }
  return true;
}

// Parses the `tab_group_color_palette_dict` dictionary. Validates whether the
// values are all integers. If they are then it loads the dictionary into
// `theme_info` and returns true. And if the check fails then it populates the
// `error` message and returns false.
bool LoadTabGroupColorPalette(
    const base::Value::Dict& theme_dict,
    // TODO(crbug.com/427972612): Take string by reference instead of taking a
    // pointer to the string.
    std::u16string* error,
    // TODO(crbug.com/427972612): Take object by reference instead of taking a
    // pointer to the object.
    ThemeInfo* theme_info) {
  DCHECK(error);
  DCHECK(theme_info);

  if (!base::FeatureList::IsEnabled(features::kCustomizeTabGroupColorPalette)) {
    return true;
  }

  const base::Value::Dict* tab_group_color_palette_dict =
      theme_dict.FindDict(keys::kThemeTabGroupColorPalette);
  if (!tab_group_color_palette_dict) {
    return true;
  }

  for (const auto [key, value] : *tab_group_color_palette_dict) {
    if (!value.is_int()) {
      *error = errors::kInvalidThemeTabGroupColorPalette;
      return false;
    }
  }

  theme_info->theme_tab_group_color_palette_ =
      tab_group_color_palette_dict->Clone();
  return true;
}

const ThemeInfo* GetInfo(const Extension* extension) {
  return static_cast<ThemeInfo*>(extension->GetManifestData(keys::kTheme));
}

}  // namespace

ThemeInfo::ThemeInfo() = default;

ThemeInfo::~ThemeInfo() = default;

// static
const ThemeInfo::ThemeImages* ThemeInfo::GetImages(const Extension* extension) {
  const ThemeInfo* theme_info = GetInfo(extension);
  return theme_info ? &theme_info->theme_images_ : nullptr;
}

// static
const base::Value::Dict* ThemeInfo::GetColors(const Extension* extension) {
  const ThemeInfo* theme_info = GetInfo(extension);
  return theme_info ? &theme_info->theme_colors_ : nullptr;
}

// static
const base::Value::Dict* ThemeInfo::GetTints(const Extension* extension) {
  const ThemeInfo* theme_info = GetInfo(extension);
  return theme_info ? &theme_info->theme_tints_ : nullptr;
}

// static
const base::Value::Dict* ThemeInfo::GetDisplayProperties(
    const Extension* extension) {
  const ThemeInfo* theme_info = GetInfo(extension);
  return theme_info ? &theme_info->theme_display_properties_ : nullptr;
}

// static
const base::Value::Dict* ThemeInfo::GetTabGroupColorPalette(
    const Extension* extension) {
  const ThemeInfo* theme_info = GetInfo(extension);
  return theme_info ? &theme_info->theme_tab_group_color_palette_ : nullptr;
}

ThemeHandler::ThemeHandler() = default;

ThemeHandler::~ThemeHandler() = default;

bool ThemeHandler::Parse(Extension* extension, std::u16string* error) {
  const base::Value::Dict* theme_dict =
      extension->manifest()->FindDictPath(keys::kTheme);
  if (!theme_dict) {
    *error = errors::kInvalidTheme;
    return false;
  }
  std::unique_ptr<ThemeInfo> theme_info(new ThemeInfo);
  std::vector<std::string> image_warnings;
  if (!LoadImages(*extension, *theme_dict, error, &image_warnings,
                  theme_info.get())) {
    return false;
  }
  if (!LoadColors(*theme_dict, error, theme_info.get())) {
    return false;
  }
  if (!LoadTints(*theme_dict, error, theme_info.get())) {
    return false;
  }
  if (!LoadDisplayProperties(*theme_dict, error, theme_info.get())) {
    return false;
  }
  if (!LoadTabGroupColorPalette(*theme_dict, error, theme_info.get())) {
    return false;
  }

  for (const auto& warning : image_warnings) {
    extension->AddInstallWarning(InstallWarning(warning, keys::kThemeImages));
  }

  extension->SetManifestData(keys::kTheme, std::move(theme_info));
  return true;
}

bool ThemeHandler::Validate(const Extension& extension,
                            std::string* error,
                            std::vector<InstallWarning>* warnings) const {
  // Validate that theme images exist.
  if (extension.is_theme()) {
    const ThemeInfo::ThemeImages* theme_images =
        extensions::ThemeInfo::GetImages(&extension);
    if (theme_images) {
      for (const auto& [theme_image_name, theme_resources] : *theme_images) {
        for (const auto& theme_resource : theme_resources) {
          base::FilePath image_path = theme_resource.resource.GetFilePath();
          if (image_path.empty() || !base::PathExists(image_path)) {
            // Bad entry.
            if (theme_resource.scale.empty()) {
              *error = l10n_util::GetStringFUTF8(
                  IDS_EXTENSION_INVALID_IMAGE_PATH,
                  theme_resource.resource.relative_path().AsUTF16Unsafe());
              return false;
            }
            // This is a warning and not a hard-error for backwards
            // compatibility with existing themes.
            warnings->emplace_back(ErrorUtils::FormatErrorMessage(
                errors::kInvalidThemeDictImagePath, theme_image_name,
                theme_resource.scale,
                theme_resource.resource.relative_path().AsUTF8Unsafe()));
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
