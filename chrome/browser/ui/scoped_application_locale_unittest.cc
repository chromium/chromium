// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/test/scoped_application_locale.h"

#include "base/i18n/icubridge/default_icu_locale.h"
#include "base/i18n/language_tag.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/test/scoped_resource_bundle_locale.h"
#include "ui/strings/grit/ui_strings.h"

namespace ui::test {
namespace {

// These unit tests are placed in `//chrome` rather than `//ui/base` so that
// they can run as part of `unit_tests`, which bundles all localized resource
// packs (`//chrome:packed_resources`). This is not ideal as the tested
// classes are defined in `//ui/base`, but allows `//ui/base` test suites to
// keep a single language without changing how test paks are bundled.

// Verifies that `ScopedApplicationLocale` updates the default ICU locale
// (observed via `base::i18n::GetDefaultIcuLocale()`) and `ui::ResourceBundle`
// localized strings across nested scopes and restores the original locale and
// resources upon destruction.
TEST(ScopedApplicationLocaleTest, SwapsLocalizedStringsAndLocales) {
  const base::i18n::LanguageTag original_tag =
      base::i18n::GetDefaultIcuLocale();

  // Verify baseline string in English.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CANCEL), u"Cancel");

  {
    ScopedApplicationLocale scoped_locale(
        base::i18n::GetKnownLanguageTag("fr"));
    EXPECT_EQ(base::i18n::GetDefaultIcuLocale(),
              base::i18n::GetKnownLanguageTag("fr"));
    // Verify that `ResourceBundle` reloads and returns localized French
    // strings.
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CANCEL), u"Annuler");

    {
      // Verify nested `ScopedApplicationLocale` restores the outer locale upon
      // destruction.
      ScopedApplicationLocale inner_locale(
          base::i18n::GetKnownLanguageTag("de"));
      EXPECT_EQ(base::i18n::GetDefaultIcuLocale(),
                base::i18n::GetKnownLanguageTag("de"));
      EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CANCEL), u"Abbrechen");
    }

    EXPECT_EQ(base::i18n::GetDefaultIcuLocale(),
              base::i18n::GetKnownLanguageTag("fr"));
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CANCEL), u"Annuler");
  }

  // Verify original English locale and resources are restored upon destruction.
  EXPECT_EQ(base::i18n::GetDefaultIcuLocale(), original_tag);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CANCEL), u"Cancel");
}

// Verifies that `ScopedResourceBundleLocale` updates the `ui::ResourceBundle`
// localized strings without changing the default ICU locale (observed via
// `base::i18n::GetDefaultIcuLocale()`), and restores the original resources
// upon destruction.
TEST(ScopedResourceBundleLocaleTest, OnlySwapsLocalizedStrings) {
  const base::i18n::LanguageTag original_tag =
      base::i18n::GetDefaultIcuLocale();

  {
    ScopedResourceBundleLocale scoped_locale(
        base::i18n::GetKnownLanguageTag("fr"));
    EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CANCEL), u"Annuler");
    EXPECT_EQ(base::i18n::GetDefaultIcuLocale(), original_tag);
  }

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_APP_CANCEL), u"Cancel");
}

}  // namespace
}  // namespace ui::test
