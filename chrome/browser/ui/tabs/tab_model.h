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

class LensOverlayController;
class TabStripModel;

namespace tabs {

class TabCollection;

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

  LensOverlayController* lens_overlay_controller() {
    return lens_overlay_controller_.get();
  }

  // Returns a pointer to the parent TabCollection. This method is specifically
  // designed to be accessible only within the collection tree that has the
  // kTabStripCollectionStorage flag enabled.
  TabCollection* GetParentCollection(base::PassKey<TabCollection>) const;

  // Provides access to the parent_collection_ for testing purposes. This method
  // bypasses the PassKey mechanism, allowing tests to simulate scenarios and
  // inspect the state without needing to replicate complex authorization
  // mechanisms.
  TabCollection* GetParentCollectionForTesting() { return parent_collection_; }

  // Updates the parent collection of the TabModel in response to structural
  // changes such as pinning, grouping, or moving the tab between collections.
  // This method ensures the TabModel remains correctly associated within the
  // tab hierarchy, maintaining consistent organization.
  void OnReparented(TabCollection* parent, base::PassKey<TabCollection>);

 private:
  std::unique_ptr<content::WebContents> contents_;

  // A back reference to the TabStripModel that contains this TabModel. The
  // owning model can be nullptr if the tab has been detached from it's previous
  // owning tabstrip model, and has yet to be transferred to a new tabstrip
  // model or is in the process of being closed.
  raw_ptr<TabStripModel> owning_model_;
  raw_ptr<content::WebContents> opener_ = nullptr;
  bool reset_opener_on_active_tab_change_ = false;
  bool pinned_ = false;
  bool blocked_ = false;
  std::optional<tab_groups::TabGroupId> group_ = std::nullopt;
  raw_ptr<TabCollection> parent_collection_ = nullptr;

  // Features that are per-tab will each have a controller.
  std::unique_ptr<LensOverlayController> lens_overlay_controller_;
};

using TabHandle = TabModel::Handle;

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_MODEL_H_
