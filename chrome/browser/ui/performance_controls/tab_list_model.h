// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_LIST_MODEL_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_LIST_MODEL_H_

#include <vector>

namespace resource_attribution {
class PageContext;
}

class TabListModel {
 public:
  explicit TabListModel(
      const std::vector<resource_attribution::PageContext>& page_contexts);
  ~TabListModel();

  void RemovePageContext(resource_attribution::PageContext tab);

  std::vector<resource_attribution::PageContext> page_contexts();

 private:
  std::vector<resource_attribution::PageContext> page_contexts_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_LIST_MODEL_H_
