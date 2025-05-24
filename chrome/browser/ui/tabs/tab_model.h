// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_MODEL_H_
#define CHROME_BROWSER_UI_TABS_TAB_MODEL_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

class TabStripModel;
namespace tabs {

class TabCollection;
class TabFeatures;

class TabModel final : public TabInterface, public TabStripModelObserver {
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

  TabStripModel* owning_model() const { return owning_model_; }
  tabs::TabInterface* opener() const { return opener_; }
  bool reset_opener_on_active_tab_change() const {
    return reset_opener_on_active_tab_change_;
  }
  bool blocked() const { return blocked_; }
  std::optional<tab_groups::TabGroupId> group() const { return group_; }

  void set_opener(tabs::TabInterface* opener) { opener_ = opener; }
  void set_reset_opener_on_active_tab_change(
      bool reset_opener_on_active_tab_change) {
    reset_opener_on_active_tab_change_ = reset_opener_on_active_tab_change;
  }

  void SetPinned(bool pinned);
  void SetGroup(std::optional<tab_groups::TabGroupId> group);

  void set_blocked(bool blocked) { blocked_ = blocked; }
  void set_split(std::optional<split_tabs::SplitTabId> split) {
    split_ = split;
  }

  void set_will_be_detaching_for_testing(bool will_be_detaching) {
    will_be_detaching_ = will_be_detaching;
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

  // Provides access to the parent_collection_ for testing purposes. This method
  // bypasses the PassKey mechanism, allowing tests to simulate scenarios and
  // inspect the state without needing to replicate complex authorization
  // mechanisms.
  TabCollection* GetParentCollectionForTesting() { return parent_collection_; }

  // Called by TabStripModel when a tab is going to be backgrounded (any
  // operation that makes the tab no longer visible, including removal from the
  // TabStripModel). Not called if TabStripModel is being destroyed.
  void WillEnterBackground(base::PassKey<TabStripModel>);

  // Called by TabStripModel when a tab is going to be detached for reinsertion
  // into a different tab strip.
  void WillDetach(base::PassKey<TabStripModel>,
                  tabs::TabInterface::DetachReason reason);

  // Called by TabStripModel when a tab has been inserted into a tab strip.
  void DidInsert(base::PassKey<TabStripModel>);

  // TabInterface overrides:
  base::WeakPtr<TabInterface> GetWeakPtr() override;
  content::WebContents* GetContents() const override;
  base::CallbackListSubscription RegisterWillDiscardContents(
      TabInterface::WillDiscardContentsCallback callback) override;
  bool IsActivated() const override;
  base::CallbackListSubscription RegisterDidActivate(
      TabInterface::DidActivateCallback callback) override;
  base::CallbackListSubscription RegisterWillDeactivate(
      TabInterface::WillDeactivateCallback callback) override;
  bool IsVisible() const override;
  base::CallbackListSubscription RegisterDidBecomeVisible(
      DidBecomeVisibleCallback callback) override;
  base::CallbackListSubscription RegisterWillBecomeHidden(
      WillBecomeHiddenCallback callback) override;

  base::CallbackListSubscription RegisterWillDetach(
      TabInterface::WillDetach callback) override;
  base::CallbackListSubscription RegisterDidInsert(
      TabInterface::DidInsertCallback callback) override;
  base::CallbackListSubscription RegisterPinnedStateChanged(
      TabInterface::PinnedStateChangedCallback callback) override;
  base::CallbackListSubscription RegisterGroupChanged(
      TabInterface::GroupChangedCallback callback) override;

  bool CanShowModalUI() const override;
  std::unique_ptr<ScopedTabModalUI> ShowModalUI() override;
  base::CallbackListSubscription RegisterModalUIChanged(
      TabInterfaceCallback callback) override;

  bool IsInNormalWindow() const override;
  BrowserWindowInterface* GetBrowserWindowInterface() override;
  const BrowserWindowInterface* GetBrowserWindowInterface() const override;
  tabs::TabFeatures* GetTabFeatures() override;
  const tabs::TabFeatures* GetTabFeatures() const override;
  bool IsPinned() const override;
  bool IsSplit() const override;
  std::optional<tab_groups::TabGroupId> GetGroup() const override;
  std::optional<split_tabs::SplitTabId> GetSplit() const override;
  TabCollection* GetParentCollection(
      base::PassKey<TabCollection>) const override;
  const TabCollection* GetParentCollection() const override;

  void OnReparented(TabCollection* parent,
                    base::PassKey<TabCollection>) override;
  void OnAncestorChanged(base::PassKey<TabCollection>) override;
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

  // Updates the tab's properties based on all of its ancestor collections.
  void UpdateProperties();

  // Tracks whether a tab-modal UI is showing.
  class ScopedTabModalUIImpl : public ScopedTabModalUI {
   public:
    explicit ScopedTabModalUIImpl(TabModel* tab);
    ~ScopedTabModalUIImpl() override;

   private:
    // Owns this. Some consumers may hold this beyond the lifetime of the tab.
    base::WeakPtr<TabModel> tab_;
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
  bool will_be_detaching_ = false;
  raw_ptr<tabs::TabInterface> opener_ = nullptr;
  bool reset_opener_on_active_tab_change_ = false;
  bool pinned_ = false;
  bool blocked_ = false;
  // TODO(crbug.com/392951786): Remove this property, and instead determine a
  // tab's split status based on whether it is part of a split tab collection.
  std::optional<split_tabs::SplitTabId> split_ = std::nullopt;
  std::optional<tab_groups::TabGroupId> group_ = std::nullopt;
  raw_ptr<TabCollection> parent_collection_ = nullptr;

  using WillDiscardContentsCallbackList = base::RepeatingCallbackList<
      void(TabInterface*, content::WebContents*, content::WebContents*)>;
  WillDiscardContentsCallbackList will_discard_contents_callback_list_;

  using DidActivateCallbackList =
      base::RepeatingCallbackList<void(TabInterface*)>;
  DidActivateCallbackList did_enter_foreground_callback_list_;

  using WillDeactivateCallbackList =
      base::RepeatingCallbackList<void(TabInterface*)>;
  WillDeactivateCallbackList will_enter_background_callback_list_;

  using DidBecomeVisibleCallback =
      base::RepeatingCallbackList<void(TabInterface*)>;
  DidActivateCallbackList did_become_visible_callback_list_;

  using WillBecomeHiddenCallback =
      base::RepeatingCallbackList<void(TabInterface*)>;
  WillBecomeHiddenCallback will_become_hidden_callback_list_;

  using WillDetachCallbackList =
      base::RepeatingCallbackList<void(TabInterface*,
                                       tabs::TabInterface::DetachReason)>;
  WillDetachCallbackList will_detach_callback_list_;

  using DidInsertCallbackList =
      base::RepeatingCallbackList<void(TabInterface*)>;
  DidInsertCallbackList did_insert_callback_list_;

  using PinnedStateChangedCallbackList =
      base::RepeatingCallbackList<void(TabInterface*, bool new_pinned_state)>;
  PinnedStateChangedCallbackList pinned_state_changed_callback_list_;

  using GroupChangedCallbackList = base::RepeatingCallbackList<
      void(TabInterface*, std::optional<tab_groups::TabGroupId> new_group)>;
  GroupChangedCallbackList group_changed_callback_list_;

  using TabInterfaceCallbackList =
      base::RepeatingCallbackList<void(TabInterface*)>;
  TabInterfaceCallbackList modal_ui_changed_callback_list_;

  // Tracks whether a modal UI is showing.
  bool showing_modal_ui_ = false;

  // Features that are per-tab will be owned by this class.
  std::unique_ptr<TabFeatures> tab_features_;

  base::WeakPtrFactory<TabModel> weak_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_MODEL_H_
