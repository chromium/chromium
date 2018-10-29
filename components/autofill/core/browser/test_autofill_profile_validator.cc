// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_profile_validator.h"

#include <memory>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/test/testdata_source.h"

namespace {
using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;
using ::i18n::addressinput::NullStorage;
using ::i18n::addressinput::TestdataSource;

}  // namespace

namespace autofill {

namespace {

std::unique_ptr<::i18n::addressinput::Source> GetInputSource() {
  base::FilePath file_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
  file_path = file_path.Append(FILE_PATH_LITERAL("third_party"))
                  .Append(FILE_PATH_LITERAL("libaddressinput"))
                  .Append(FILE_PATH_LITERAL("src"))
                  .Append(FILE_PATH_LITERAL("testdata"))
                  .Append(FILE_PATH_LITERAL("countryinfo.txt"));
  return std::make_unique<TestdataSource>(true, file_path.AsUTF8Unsafe());
}

std::unique_ptr<::i18n::addressinput::Storage> GetInputStorage() {
  return std::unique_ptr<Storage>(new NullStorage);
}

}  // namespace

// static
AutofillProfileValidator* TestAutofillProfileValidator::GetInstance() {
  static base::LazyInstance<TestAutofillProfileValidator>::DestructorAtExit
      instance = LAZY_INSTANCE_INITIALIZER;
  return &(instance.Get().autofill_profile_validator_);
}

TestAutofillProfileValidator::TestAutofillProfileValidator()
    : autofill_profile_validator_(GetInputSource(), GetInputStorage()) {}

TestAutofillProfileValidator::~TestAutofillProfileValidator() {}

}  // namespace autofill
