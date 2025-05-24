// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_test_utils.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

void SetID(WebContents* contents, int id) {
  contents->SetUserData(&kTabStripModelTestIDUserDataKey,
                        std::make_unique<TabStripModelTestIDUserData>(id));
}

int GetID(WebContents* contents) {
  TabStripModelTestIDUserData* user_data =
      static_cast<TabStripModelTestIDUserData*>(
          contents->GetUserData(&kTabStripModelTestIDUserDataKey));

  return user_data ? user_data->id() : -1;
}

void PrepareTabstripForSelectionTest(
    base::OnceCallback<void(int)> add_tabs_callback,
    TabStripModel* model,
    int tab_count,
    int pinned_count,
    const std::vector<int> selected_tabs) {
  std::move(add_tabs_callback).Run(tab_count);
  for (int i = 0; i < pinned_count; ++i) {
    model->SetTabPinned(i, true);
  }

  ui::ListSelectionModel selection_model;
  for (const int selected_tab : selected_tabs) {
    selection_model.AddIndexToSelection(selected_tab);
  }
  selection_model.set_active(*selection_model.selected_indices().begin());
  model->SetSelectionFromModel(selection_model);
}

std::string GetTabStripStateString(const TabStripModel* model) {
  std::string actual;
  for (int i = 0; i < model->count(); ++i) {
    if (i > 0) {
      actual += " ";
    }

    actual += base::NumberToString(GetID(model->GetWebContentsAt(i)));

    if (model->IsTabPinned(i)) {
      actual += "p";
    }

    if (model->GetSplitForTab(i).has_value()) {
      actual += "s";
    }
  }
  return actual;
}
