// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_EVENT_MATCHERS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_EVENT_MATCHERS_H_

#include "components/browser_apis/tab_strip/tab_strip_api_events.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace tabs_api {

MATCHER_P(CollectionCreated, predicate, "") {
  return arg->is_collection_created_event() &&
         predicate(arg->get_collection_created_event());
}

MATCHER_P(TabGroupChanged, predicate, "") {
  return arg->is_data_changed_event() &&
         arg->get_data_changed_event()->is_tab_group() &&
         predicate(arg->get_data_changed_event()->get_tab_group());
}

MATCHER_P(NodeMoved, predicate, "") {
  return arg->is_node_moved_event() && predicate(arg->get_node_moved_event());
}

MATCHER_P(NodesClosed, predicate, "") {
  return arg->is_nodes_closed_event() &&
         predicate(arg->get_nodes_closed_event());
}

MATCHER_P(TabsCreated, predicate, "") {
  return arg->is_tabs_created_event() &&
         predicate(arg->get_tabs_created_event());
}

MATCHER_P(TabChanged, predicate, "") {
  return arg->is_data_changed_event() &&
         arg->get_data_changed_event()->is_tab() &&
         predicate(arg->get_data_changed_event()->get_tab());
}

}  // namespace tabs_api

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TESTING_EVENT_MATCHERS_H_
