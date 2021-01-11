// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUBBLE_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUBBLE_VIEW_MODEL_H_

#include <vector>
#include "base/strings/string16.h"
#include "components/version_info/channel.h"

// Currently there are differences in both visible name and visible description
// between about_flags and what we want for Chrome Labs. We are coordinating to
// match these. Visible name and visible description can be removed from this
// struct after that.
struct LabInfo {
  LabInfo(const std::string& internal_name,
          const base::string16& visible_name,
          const base::string16& visible_description,
          version_info::Channel allowed_channel);
  LabInfo(const LabInfo& other);
  ~LabInfo();
  std::string internal_name;
  base::string16 visible_name;
  base::string16 visible_description;
  // Channels that are less stable than allowed_channel will also be
  // considered allowed. ex) if BETA is specified, this feature will also be
  // shown on CANARY and DEV.
  version_info::Channel allowed_channel;
};

class ChromeLabsBubbleViewModel {
 public:
  ChromeLabsBubbleViewModel();
  // |lab_info_| will have `lab_info`, and whatever SetUpLabs appends to it.
  explicit ChromeLabsBubbleViewModel(const std::vector<LabInfo>& lab_info);
  ~ChromeLabsBubbleViewModel();

  const std::vector<LabInfo>& GetLabInfo() const;

  void SetLabInfoForTesting(const std::vector<LabInfo>& test_feature_info);

 private:
  void SetUpLabs();

  std::vector<LabInfo> lab_info_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUBBLE_VIEW_MODEL_H_
