// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_profile_validator_delayed.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/cancelable_callback.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace autofill {
namespace {

const int kRulesDelayedLoadingTimeSeconds = 3;

}  // namespace

TestAutofillProfileValidatorDelayed::TestAutofillProfileValidatorDelayed(
    std::unique_ptr<::i18n::addressinput::Source> source,
    std::unique_ptr<::i18n::addressinput::Storage> storage)
    : AutofillProfileValidator(std::move(source), std::move(storage)) {}

TestAutofillProfileValidatorDelayed::~TestAutofillProfileValidatorDelayed() {}

void TestAutofillProfileValidatorDelayed::LoadRulesInstantly(
    const std::string& region_code) {
  address_validator_.LoadRules(region_code);
}

void TestAutofillProfileValidatorDelayed::LoadRulesForRegion(
    const std::string& region_code) {
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TestAutofillProfileValidatorDelayed::LoadRulesInstantly,
                     base::Unretained(this), region_code),
      base::Seconds(kRulesDelayedLoadingTimeSeconds));
}
}  // namespace autofill
