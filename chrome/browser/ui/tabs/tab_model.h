// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_MODEL_H_
#define CHROME_BROWSER_UI_TABS_TAB_MODEL_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/supports_handles.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/web_contents.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

class TabStripModel;

namespace tabs {

class TabCollection;
class TabFeatures;

class TabModel final : public SupportsHandles<TabModel>,
                       public TabInterface,
                       public TabStripModelObserver {
 public:
  // Conceptually, tabs should always be a part of a normal window. There are
  // currently 2 cases where they are not:
  // (1) Tabbed PWAs is a ChromeOS_only feature that exposes Tabs to PWAs.
  // (2) Non-browser windows currently have a tab-strip and may use tabs. See
  // TODO(https://crbug.com/331031753) which tracks their eventual removal.
  // TODO(https://crbug.com/346692548): Tabs should never be constructed in
  // isolation of a model.
  TabModel(std::unique_ptr<content::WebContents> contents,
           TabStripModel* soon_to_be_owning_model);
  ~TabModel() override;

  TabModel(const TabModel&) = delete;
  TabModel& operator=(const TabModel&) = delete;

  void OnAddedToModel(TabStripModel* owning_model);
  void OnRemovedFromModel();

  content::WebContents* contents() const { return contents_.get(); }
  TabStripModel* owning_model() const { return owning_model_.get(); }
  tabs::TabModel* opener() const { return opener_; }
  bool reset_opener_on_active_tab_change() const {
    return reset_opener_on_active_tab_change_;
  }
  bool pinned() const { return pinned_; }
  bool blocked() const { return blocked_; }
  std::optional<tab_groups::TabGroupId> group() const { return group_; }

  void set_opener(tabs::TabModel* opener) { opener_ = opener; }
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

  // https://crbug.com/331022416: Do not use this method. This is only used by
  // tab discard, which is being refactored to not need this.
  std::unique_ptr<content::WebContents> DiscardContents(
      std::unique_ptr<content::WebContents> contents);

  // This destroys the TabModel and takes ownership of the underlying
  // WebContents.
  static std::unique_ptr<content::WebContents> DestroyAndTakeWebContents(
      std::unique_ptr<TabModel> tab_model);

  // When a tab is going to be removed from the tabstrip in preparation for
  // destruction, `TabFeatures` should be destroyed first to ensure individual
  // features do not need to handle the situation of existing outside the
  // context of a tab strip.
  void DestroyTabFeatures();
  TabFeatures* tab_features() { return tab_features_.get(); }

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

  // Called by TabStripModel when a tab is going to be backgrounded (any
  // operation that makes the tab no longer visible, including removal from the
  // TabStripModel). Not called if TabStripModel is being destroyed.
  void WillEnterBackground(base::PassKey<TabStripModel>);

  // Called by TabStripModel when a tab is going to be detached for reinsertion
  // into a different tab strip.
  void WillDetach(base::PassKey<TabStripModel>,
                  tabs::TabInterface::DetachReason reason);

  // TabInterface overrides:
  content::WebContents* GetContents() const override;
  base::CallbackListSubscription RegisterWillDiscardContents(
      TabInterface::WillDiscardContentsCallback callback) override;
  bool IsInForeground() const override;
  base::CallbackListSubscription RegisterDidEnterForeground(
      TabInterface::DidEnterForegroundCallback callback) override;
  base::CallbackListSubscription RegisterWillEnterBackground(
      TabInterface::WillEnterBackgroundCallback callback) override;
  base::CallbackListSubscription RegisterWillDetach(
      TabInterface::WillDetach callback) override;
  bool CanShowModalUI() const override;
  std::unique_ptr<ScopedTabModalUI> ShowModalUI() override;
  bool IsInNormalWindow() const override;
  BrowserWindowInterface* GetBrowserWindowInterface() override;
  tabs::TabFeatures* GetTabFeatures() override;
  uint32_t GetTabHandle() override;
  void Close() override;

 private:
  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // TODO(https://crbug.com/346692548): This will not be necessary once
  // soon_to_be_owning_model_ is removed. TabInterface logic can only be invoked
  // in contexts where a model exists.
  TabStripModel* GetModelForTabInterface() const;

  // Tracks whether a tab-modal UI is showing.
  class ScopedTabModalUIImpl : public ScopedTabModalUI {
   public:
    explicit ScopedTabModalUIImpl(TabModel* tab);
    ~ScopedTabModalUIImpl() override;

   private:
    // Owns this.
    raw_ptr<TabModel> tab_;
  };

  // This must always be the first member so that it is destroyed last. This is
  // because there are some instances where a caller may want to destroy a
  // TabModel but keep the WebContents alive. There are other destructors such
  // as TabFeatures that may require a valid `contents_` during destruction.
  std::unique_ptr<content::WebContents> contents_owned_;
  raw_ptr<content::WebContents> contents_;

  // A back reference to the TabStripModel that contains this TabModel. The
  // owning model can be nullptr if the tab has been detached from it's previous
  // owning tabstrip model, and has yet to be transferred to a new tabstrip
  // model or is in the process of being closed.
  raw_ptr<TabStripModel> owning_model_ = nullptr;
  raw_ptr<TabStripModel> soon_to_be_owning_model_ = nullptr;
  raw_ptr<tabs::TabModel> opener_ = nullptr;
  bool reset_opener_on_active_tab_change_ = false;
  bool pinned_ = false;
  bool blocked_ = false;
  std::optional<tab_groups::TabGroupId> group_ = std::nullopt;
  raw_ptr<TabCollection> parent_collection_ = nullptr;

  using WillDiscardContentsCallbackList = base::RepeatingCallbackList<
      void(TabInterface*, content::WebContents*, content::WebContents*)>;
  WillDiscardContentsCallbackList will_discard_contents_callback_list_;

  using DidEnterForegroundCallbackList =
      base::RepeatingCallbackList<void(TabInterface*)>;
  DidEnterForegroundCallbackList did_enter_foreground_callback_list_;

  using WillEnterBackgroundCallbackList =
      base::RepeatingCallbackList<void(TabInterface*)>;
  WillEnterBackgroundCallbackList will_enter_background_callback_list_;

  using WillDetachCallbackList =
      base::RepeatingCallbackList<void(TabInterface*,
                                       tabs::TabInterface::DetachReason)>;
  WillDetachCallbackList will_detach_callback_list_;

  // Tracks whether a modal UI is showing.
  bool showing_modal_ui_ = false;

  // Features that are per-tab will be owned by this class.
  std::unique_ptr<TabFeatures> tab_features_;
};

using TabHandle = TabModel::Handle;

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_MODEL_H_
