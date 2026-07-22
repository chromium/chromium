// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESSES_ANDROID_SUPPORTED_COUNTRIES_CACHE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESSES_ANDROID_SUPPORTED_COUNTRIES_CACHE_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "components/autofill/core/browser/ui/addresses/android/dropdown_key_value_android.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace autofill {

// Memoizes the localized list of supported countries per application locale.
//
// Building the list is expensive on Android: it resolves every region code
// (~250) to a localized country name, and each resolution is a JNI round-trip
// into a platform-ICU lookup. The result depends only on the locale, which is
// stable for the lifetime of the process, so it is worth caching.
//
// The list construction is injected as a `Builder` callback. This keeps the
// class free of localization dependencies and makes the caching behavior
// unit-testable with a fake builder.
class COMPONENT_EXPORT(AUTOFILL) SupportedCountriesCache {
 public:
  // Builds the supported-country list for `locale`. Invoked at most once per
  // distinct locale.
  using Builder = base::RepeatingCallback<std::vector<DropdownKeyValueAndroid>(
      std::string_view locale)>;

  explicit SupportedCountriesCache(Builder builder);
  SupportedCountriesCache(const SupportedCountriesCache&) = delete;
  SupportedCountriesCache& operator=(const SupportedCountriesCache&) = delete;
  ~SupportedCountriesCache();

  // Returns the supported-country list for `locale`, building it via the
  // injected builder on the first request for that locale and serving cached
  // copies afterwards. Must be called on the same sequence for the lifetime
  // of the instance; the UI-thread contract is inherited from the sole caller
  // (JNI_AutofillProfileBridge_GetSupportedCountries), which reads the locale
  // from g_browser_process->GetApplicationLocale() -- itself guarded by a
  // SEQUENCE_CHECKER on the browser process UI sequence.
  std::vector<DropdownKeyValueAndroid> GetForLocale(std::string_view locale);

 private:
  const Builder builder_;

  absl::flat_hash_map<std::string, std::vector<DropdownKeyValueAndroid>> cache_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_ADDRESSES_ANDROID_SUPPORTED_COUNTRIES_CACHE_H_
