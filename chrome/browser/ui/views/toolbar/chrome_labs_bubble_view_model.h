// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUBBLE_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUBBLE_VIEW_MODEL_H_

#include <vector>
#include "base/strings/string16.h"

class ChromeLabsBubbleViewModel {
 public:
  ChromeLabsBubbleViewModel();
  ~ChromeLabsBubbleViewModel();

  std::vector<std::string> GetLabInfo() const;

 private:
  void SetUpLabs();

  std::vector<std::string> lab_internal_names_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_CHROME_LABS_BUBBLE_VIEW_MODEL_H_
