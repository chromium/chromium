// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"

#include <optional>

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

std::optional<std::vector<LabInfo>>& GetTestData() {
  static base::NoDestructor<std::optional<std::vector<LabInfo>>> test_lab_data;
  return *test_lab_data;
}

void SetLabInfoForTesting(const std::vector<LabInfo>& test_feature_info) {
  if (test_feature_info.empty()) {
    GetTestData().reset();
  } else {
    GetTestData() = test_feature_info;
  }
}

// TODO(elainechien): Explore better ways to allow developers to add their
// experiments.
// Experiments featured in labs must have feature entries of type FEATURE_VALUE
// (Default, Enabled, Disabled states) or FEATURE_WITH_PARAMS_VALUE
const std::vector<LabInfo>& GetData() {
  if (GetTestData()) {
    return GetTestData().value();
  }

  static const base::NoDestructor<std::vector<LabInfo>> lab_info_(
      []() { return std::vector<LabInfo>(); }());

  return *lab_info_;
}
}  // namespace

LabInfo::LabInfo(
    const std::string& internal_name,
    const std::u16string& visible_name,
    const std::u16string& visible_description,
    const std::string& feedback_category_name,
    version_info::Channel allowed_channel,
    std::vector<std::u16string> translated_feature_variation_descriptions)
    : internal_name(internal_name),
      visible_name(visible_name),
      visible_description(visible_description),
      feedback_category_name(feedback_category_name),
      allowed_channel(allowed_channel),
      translated_feature_variation_descriptions(
          translated_feature_variation_descriptions) {}

LabInfo::LabInfo(const LabInfo& other) = default;

LabInfo::~LabInfo() = default;

ChromeLabsModel::ChromeLabsModel() : lab_info_(GetData()) {}

ChromeLabsModel::~ChromeLabsModel() = default;

// static
ChromeLabsModel* ChromeLabsModel::GetInstance() {
  static base::NoDestructor<ChromeLabsModel> instance;
  return instance.get();
}

const std::vector<LabInfo>& ChromeLabsModel::GetLabInfo() const {
  return *lab_info_;
}

ScopedChromeLabsModelDataForTesting::ScopedChromeLabsModelDataForTesting() =
    default;

ScopedChromeLabsModelDataForTesting::~ScopedChromeLabsModelDataForTesting() {
  // Remove test data.
  SetLabInfoForTesting(std::vector<LabInfo>());  // IN-TEST
}

void ScopedChromeLabsModelDataForTesting::SetModelDataForTesting(
    const std::vector<LabInfo>& test_feature_info) {
  SetLabInfoForTesting(test_feature_info);  // IN-TEST
}
