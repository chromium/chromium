// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TAB_MODEL_EVENT_BRIDGE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TAB_MODEL_EVENT_BRIDGE_H_

#include "base/observer_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_api/adapters/event_bridge.h"
#include "components/tabs/public/tab_collection_observer.h"

namespace tabs_api {

class AndroidTabStripModelAdapter;
class TranslationAdapter;

class AndroidTabModelEventBridge : public EventBridge,
                                   public TabModelObserver,
                                   public tabs::TabCollectionObserver {
 public:
  AndroidTabModelEventBridge(TabModel* model,
                             AndroidTabStripModelAdapter& adapter,
                             TranslationAdapter& translation_adapter);
  ~AndroidTabModelEventBridge() override;

  // EventBridge:
  void AddObserver(events::EventObserver* observer) override;
  void RemoveObserver(events::EventObserver* observer) override;

  // TabModelObserver:
  void DidSelectTab(TabAndroid* tab, TabModel::TabSelectionType type) override;
  void DidAddTab(TabAndroid* tab, TabModel::TabLaunchType type) override;
  void DidRemoveTabForClosure(TabAndroid* tab) override;
  void TabRemoved(TabAndroid* tab) override;
  void OnTabGroupCreated(tab_groups::TabGroupId group_id) override;
  void OnTabGroupRemoving(tab_groups::TabGroupId group_id) override;
  void OnTabGroupVisualsChanged(tab_groups::TabGroupId group_id) override;

  // TabCollectionObserver:
  void OnChildMoved(const tabs::TabCollection::Position& to_position,
                    const NodeData& node_data) override;

 private:
  void Notify(events::Event event) const;

  base::ObserverList<events::EventObserver> observers_;
  raw_ptr<TabModel> model_;
  raw_ref<AndroidTabStripModelAdapter> adapter_;
  raw_ref<TranslationAdapter> translation_adapter_;

  // Android tab model does selection change event does not contain the last
  // selected tab, so we will need to keep track of it ourselves.
  tabs::TabHandle last_selected_tab_;
};

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_ANDROID_TAB_MODEL_IMPL_ANDROID_TAB_MODEL_EVENT_BRIDGE_H_
