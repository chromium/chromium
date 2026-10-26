// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_app_definition_utils.h"

#include "base/numerics/safe_conversions.h"
#include "chrome/browser/browser_process.h"
#include "third_party/icu/source/common/unicode/localematcher.h"
#include "ui/base/resource/resource_bundle.h"

namespace web_app {

namespace {

icu::Locale GetLocaleFromTranslation(const Translation& translation) {
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale locale =
      icu::Locale::forLanguageTag(translation.bcp47_locale, error);
  CHECK(U_SUCCESS(error), base::NotFatalUntil::M162);
  return locale;
}

}  // namespace

OrderedSizeToBitmap LoadBundledIcons(
    const std::initializer_list<int>& icon_resource_ids) {
  OrderedSizeToBitmap results;
  for (int id : icon_resource_ids) {
    const gfx::Image& image =
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(id);
    CHECK_EQ(image.Width(), image.Height(), base::NotFatalUntil::M162);
    results[image.Width()] = image.AsBitmap();
  }
  return results;
}

const char* GetTranslatedName(const char* utf8_default_name,
                              base::span<const Translation> translations) {
  UErrorCode error = U_ZERO_ERROR;
  icu::LocaleMatcher matcher = icu::LocaleMatcher::Builder()
                                   .setSupportedLocalesViaConverter(
                                       translations.begin(), translations.end(),
                                       GetLocaleFromTranslation)
                                   .build(error);
  CHECK(U_SUCCESS(error), base::NotFatalUntil::M162);

  icu::Locale application_locale = icu::Locale::forLanguageTag(
      g_browser_process->GetApplicationLocale(), error);
  CHECK(U_SUCCESS(error), base::NotFatalUntil::M162);

  int32_t best_index =
      matcher.getBestMatchResult(application_locale, error).getSupportedIndex();
  CHECK(U_SUCCESS(error), base::NotFatalUntil::M162);

  if (best_index == -1)
    return utf8_default_name;

  return translations[base::checked_cast<size_t>(best_index)].utf8_translation;
}

}  // namespace web_app
