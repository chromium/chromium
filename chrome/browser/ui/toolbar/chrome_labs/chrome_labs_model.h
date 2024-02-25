// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_CHROME_LABS_CHROME_LABS_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_CHROME_LABS_CHROME_LABS_MODEL_H_

#include <string>
#include <vector>
#include "base/memory/raw_ref.h"
#include "components/version_info/channel.h"

struct LabInfo {
  LabInfo(
      const std::string& internal_name,
      const std::u16string& visible_name,
      const std::u16string& visible_description,
      const std::string& feedback_category_name,
      version_info::Channel allowed_channel,
      std::vector<std::u16string> translated_feature_variation_descriptions =
          std::vector<std::u16string>());
  LabInfo(const LabInfo& other);
  ~LabInfo();
  std::string internal_name;
  std::u16string visible_name;
  std::u16string visible_description;
  std::string feedback_category_name;
  // Channels that are less stable than allowed_channel will also be
  // considered allowed. ex) if BETA is specified, this feature will also be
  // shown on CANARY and DEV.
  version_info::Channel allowed_channel;
  std::vector<std::u16string> translated_feature_variation_descriptions;
};

class ChromeLabsModel {
 public:
  ChromeLabsModel();
  ~ChromeLabsModel();

  const std::vector<LabInfo>& GetLabInfo() const;

 private:
  const raw_ref<const std::vector<LabInfo>> lab_info_;
};

// ScopedChromeLabsModelDataForTesting is intended to be used in test settings
// to replace the production data in ChromeLabsModel. Upon destruction,
// ScopedChromeLabsModelDataForTesting will remove the test data from
// ChromeLabsModel.

class ScopedChromeLabsModelDataForTesting {
 public:
  ScopedChromeLabsModelDataForTesting();
  ScopedChromeLabsModelDataForTesting(
      const ScopedChromeLabsModelDataForTesting&) = delete;
  ScopedChromeLabsModelDataForTesting& operator=(
      const ScopedChromeLabsModelDataForTesting&) = delete;
  ~ScopedChromeLabsModelDataForTesting();

  void SetModelDataForTesting(const std::vector<LabInfo>& test_feature_info);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_CHROME_LABS_CHROME_LABS_MODEL_H_
