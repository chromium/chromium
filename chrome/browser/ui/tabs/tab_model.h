// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_MODEL_H_
#define CHROME_BROWSER_UI_TABS_TAB_MODEL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/supports_handles.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/web_contents.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

class TabStripModel;

class TabModel final : public SupportsHandles<const TabModel> {
 public:
  TabModel(std::unique_ptr<content::WebContents> contents,
           TabStripModel* owning_model);
  ~TabModel() override;

  TabModel(const TabModel&) = delete;
  TabModel& operator=(const TabModel&) = delete;

  void OnAddedToModel(TabStripModel* owning_model);
  void OnRemovedFromModel();

  content::WebContents* contents() const { return contents_.get(); }
  TabStripModel* owning_model() const { return owning_model_.get(); }
  content::WebContents* opener() const { return opener_; }
  bool reset_opener_on_active_tab_change() const {
    return reset_opener_on_active_tab_change_;
  }
  bool pinned() const { return pinned_; }
  bool blocked() const { return blocked_; }
  std::optional<tab_groups::TabGroupId> group() const { return group_; }

  void set_contents(std::unique_ptr<content::WebContents> contents) {
    contents_ = std::move(contents);
  }
  void set_opener(content::WebContents* opener) { opener_ = opener; }
  void set_reset_opener_on_active_tab_change(
      bool reset_opener_on_active_tab_change) {
    reset_opener_on_active_tab_change_ = reset_opener_on_active_tab_change;
  }
  void set_pinned(bool pinned) { pinned_ = pinned; }
  void set_blocked(bool blocked) { blocked_ = blocked; }
  void set_group(std::optional<tab_groups::TabGroupId> group) {
    group_ = group;
  }

  void WriteIntoTrace(perfetto::TracedValue context) const;

  std::unique_ptr<content::WebContents> ReplaceContents(
      std::unique_ptr<content::WebContents> contents) {
    contents_.swap(contents);
    return contents;
  }

 private:
  std::unique_ptr<content::WebContents> contents_;
  // A back reference to the TabStripModel that contains this TabModel.
  raw_ptr<TabStripModel> owning_model_ = nullptr;
  raw_ptr<content::WebContents> opener_ = nullptr;
  bool reset_opener_on_active_tab_change_ = false;
  bool pinned_ = false;
  bool blocked_ = false;
  std::optional<tab_groups::TabGroupId> group_ = std::nullopt;
};

using TabHandle = TabModel::Handle;

#endif  // CHROME_BROWSER_UI_TABS_TAB_MODEL_H_
