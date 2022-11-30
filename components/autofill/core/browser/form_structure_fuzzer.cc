// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

namespace {

struct TestCase {
  // TestCase constructor is the place for all one-time initialization needed
  // for the fuzzer.
  TestCase() {
    // Init command line because otherwise the autofill code accessing it will
    // crash.
    base::CommandLine::Init(0, nullptr);
  }
};

TestCase* test_case = new TestCase();

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FormData form_data;
  form_data.fields.push_back(FormFieldData());
  form_data.fields.back().autocomplete_attribute.assign(
      reinterpret_cast<const char*>(data), size);
  FormStructure form_structure(form_data);
  form_structure.SetFieldTypesFromAutocompleteAttribute();
  return 0;
}

}  // namespace autofill
