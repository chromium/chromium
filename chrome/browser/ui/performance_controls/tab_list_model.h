// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_LIST_MODEL_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_LIST_MODEL_H_

#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace resource_attribution {
class PageContext;
}

class TabListModel {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called immediately after the tab count changes.
    virtual void OnTabCountChanged(int count) {}
  };

  explicit TabListModel(
      const std::vector<resource_attribution::PageContext>& page_contexts);
  ~TabListModel();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void RemovePageContext(resource_attribution::PageContext tab);

  std::vector<resource_attribution::PageContext> page_contexts();

  int count();

 private:
  std::vector<resource_attribution::PageContext> page_contexts_;
  base::ObserverList<Observer> tab_list_model_observers_;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TAB_LIST_MODEL_H_
