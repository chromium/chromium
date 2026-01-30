// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android.h"

TabModelObserver::TabModelObserver() = default;

TabModelObserver::~TabModelObserver() = default;

void TabModelObserver::DidSelectTab(TabAndroid* tab,
                                    TabModel::TabSelectionType type) {}

void TabModelObserver::WillCloseTab(TabAndroid* tab) {}

void TabModelObserver::DidRemoveTabForClosure(TabAndroid* tab) {}

void TabModelObserver::OnFinishingTabClosure(
    TabAndroid* tab,
    TabModel::TabClosingSource source) {}

void TabModelObserver::OnFinishingMultipleTabClosure(
    const std::vector<TabAndroid*>& tabs,
    bool canRestore) {}

void TabModelObserver::WillAddTab(TabAndroid* tab,
                                  TabModel::TabLaunchType type) {}

void TabModelObserver::DidAddTab(TabAndroid* tab,
                                 TabModel::TabLaunchType type) {}

void TabModelObserver::DidMoveTab(TabAndroid* tab,
                                  int new_index,
                                  int old_index) {}

void TabModelObserver::OnTabClosePending(const std::vector<TabAndroid*>& tabs,
                                         TabModel::TabClosingSource source) {}

void TabModelObserver::TabClosureUndone(TabAndroid* tab) {}

void TabModelObserver::OnTabCloseUndone(const std::vector<TabAndroid*>& tabs) {}

void TabModelObserver::TabClosureCommitted(TabAndroid* tab) {}

void TabModelObserver::AllTabsClosureCommitted() {}

void TabModelObserver::AllTabsAreClosing() {}

void TabModelObserver::TabRemoved(TabAndroid* tab) {}

void TabModelObserver::OnTabGroupCreated(tab_groups::TabGroupId group_id) {}

void TabModelObserver::OnTabGroupRemoving(tab_groups::TabGroupId group_id) {}

void TabModelObserver::OnTabGroupMoved(tab_groups::TabGroupId group_id,
                                       int old_index) {}

void TabModelObserver::OnTabGroupVisualsChanged(
    tab_groups::TabGroupId group_id) {}
