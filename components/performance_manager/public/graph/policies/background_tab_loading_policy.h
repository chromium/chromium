// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_POLICIES_BACKGROUND_TAB_LOADING_POLICY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_POLICIES_BACKGROUND_TAB_LOADING_POLICY_H_

namespace performance_manager {

namespace policies {

// Schedules the restored WebContents in |web_contents| to be loaded when
// appropriate. Invoked from the UI thread.
void ScheduleLoadForRestoredTabs(
    std::vector<content::WebContents*> web_contents);

}  // namespace policies

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_GRAPH_POLICIES_BACKGROUND_TAB_LOADING_POLICY_H_
