// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <tuple>
#include <vector>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/determine_regex_types.h"
#include "components/autofill/core/browser/form_qualifiers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_fuzzed_producer.h"
#include "components/autofill/core/common/form_field_data.h"
#include "third_party/icu/fuzzers/fuzzer_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/ui_base_paths.h"

namespace autofill {

namespace {

struct TestCase {
  // TestCase constructor is the place for all one-time initialization needed
  // for the fuzzer.
  TestCase() {
    // Init command line because otherwise the autofill code accessing it will
    // crash.
    base::CommandLine::Init(0, nullptr);

    // Load the resource assets needed for the autofill code.
    ui::RegisterPathProvider();
#if BUILDFLAG(IS_ANDROID)
    // Override DIR_RESOURCE_PAKS_ANDROID to DIR_ASSETS to load files pushed to
    // the shared public test data directory instead of packaging them in the
    // APK, saving space and maintaining consistency with other Android fuzzers.
    base::FilePath assets_dir;
    base::PathService::Get(base::DIR_ASSETS, &assets_dir);
    base::PathService::Override(ui::DIR_RESOURCE_PAKS_ANDROID, assets_dir);
#endif
    ui::ResourceBundle::InitSharedInstanceWithPakPath(
        base::PathService::CheckedGet(ui::UI_TEST_PAK));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("components_tests_resources.pak"),
        ui::kScaleFactorNone);
  }

  // Used by `ResourceBundle`.
  base::AtExitManager at_exit_manager;
  // ICU is needed for the autofill code.
  IcuEnvironment icu_environment;
};

GeoIpCountryCode GenerateGeoIpCountryCode(FuzzedDataProvider& data_provider) {
  char chars[2];
  for (auto& letter : chars) {
    letter = data_provider.ConsumeIntegralInRange('A', 'Z');
  }
  return GeoIpCountryCode(std::string(std::begin(chars), std::end(chars)));
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static const base::NoDestructor<TestCase> test_case;
  FuzzedDataProvider data_provider(data, size);
  FormData form_data = GenerateFormData(data_provider);

  FormStructure form_structure(form_data);
  const RegexPredictions regex_predictions = DetermineRegexTypes(
      GenerateGeoIpCountryCode(data_provider), LanguageCode(""), form_data,
      /*log_manager=*/nullptr, /*ignore_small_forms=*/true);
  regex_predictions.ApplyTo(form_structure.fields());
  form_structure.RationalizeAndAssignSections(
      GenerateGeoIpCountryCode(data_provider), LanguageCode(""),
      /*log_manager=*/nullptr);
  form_structure.RationalizeAndAssignSections(
      GenerateGeoIpCountryCode(data_provider), LanguageCode(""),
      /*log_manager=*/nullptr);
  std::ignore = IsAutofillable(form_structure);
  std::ignore = form_structure.IsCompleteCreditCardForm(
      FormStructure::CreditCardFormCompleteness::kCompleteCreditCardForm);
  std::ignore = ShouldBeParsed(form_structure, /*log_manager=*/nullptr);
  std::ignore =
      ShouldRunHeuristics(form_structure, /*ignore_small_forms=*/true);
  std::ignore = ShouldRunHeuristicsForSingleFields(form_structure);
  std::ignore = ShouldBeQueried(form_structure);
  std::ignore = ShouldBeUploaded(form_structure);
  return 0;
}

}  // namespace autofill
