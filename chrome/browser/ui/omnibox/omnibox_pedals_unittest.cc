// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

// Note: Pedals have their own components unit tests, which should be
// the preferred place for testing the classes. The tests here are for
// testing things that depend on Chrome resources, for example GRIT strings.

TEST(OmniboxPedals, DataLoadsForAllLocales) {
  // Locale selection is platform sensitive. On Linux, environment is used.
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  MockAutocompleteProviderClient client;

  // This is an exhaustive list of pedals-supported locales used to ensure
  // translation data for each loads below.
  const std::string locales[] = {
      "am",    "ar",    "bg", "bn",     "ca", "cs",    "da",    "de",  "el",
      "en",    "en-GB", "es", "es-419", "et", "fa",    "fi",    "fil", "fr",
      "gu",    "he",    "hi", "hr",     "hu", "id",    "it",    "ja",  "kn",
      "ko",    "lt",    "lv", "ml",     "mr", "ms",    "nl",    "pl",  "pt-BR",
      "pt-PT", "ro",    "ru", "sk",     "sl", "sr",    "sv",    "sw",  "ta",
      "te",    "th",    "tr", "uk",     "vi", "zh-CN", "zh-TW",
  };
  for (const std::string& locale : locales) {
    // Prepare the shared ResourceBundle with data for tested locale.
    env->SetVar("LANG", locale);
    ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(locale);

    // Instantiating the provider loads concept data from shared ResourceBundle,
    // or from omnibox_pedal_synonyms.grd if using translation console.
    // Note, with translation console process, we don't have specific cover
    // cases to test, and there is no fallback to English. Just instantiating
    // the provider above confirms that any available translation data loads
    // because the `OmniboxPedalProvider` ctor loads, parses, transforms, and
    // checks all trigger grit strings.
    client.set_pedal_provider(std::make_unique<OmniboxPedalProvider>(
        client,
        GetPedalImplementations(client.IsIncognitoProfile(),
                                client.IsGuestSession(), /*testing=*/true)));
    EXPECT_EQ(client.GetPedalProvider()->FindPedalMatch(u""), nullptr);
  }
}
