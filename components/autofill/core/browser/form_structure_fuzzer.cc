// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <fuzzer/FuzzedDataProvider.h>

#include <string>
#include <tuple>
#include <vector>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_structure.h"
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

TestCase* test_case = new TestCase();

GeoIpCountryCode GenerateGeoIpCountryCode(FuzzedDataProvider& data_provider) {
  char chars[2];
  for (auto& letter : chars) {
    letter = data_provider.ConsumeIntegralInRange('A', 'Z');
  }
  return GeoIpCountryCode(std::string(std::begin(chars), std::end(chars)));
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);
  FormData form_data = GenerateFormData(data_provider);

  FormStructure form_structure(form_data);
  form_structure.DetermineHeuristicTypes(
      GenerateGeoIpCountryCode(data_provider),
      /*form_interactions_ukm_logger=*/nullptr,
      /*log_manager=*/nullptr);
  std::ignore = form_structure.IsAutofillable();
  std::ignore = form_structure.IsCompleteCreditCardForm();
  std::ignore = form_structure.ShouldBeParsed();
  std::ignore = form_structure.ShouldRunHeuristics();
  std::ignore = form_structure.ShouldRunHeuristicsForSingleFieldForms();
  std::ignore = form_structure.ShouldBeQueried();
  std::ignore = form_structure.ShouldBeUploaded();
  return 0;
}

}  // namespace autofill
