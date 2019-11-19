// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/navigation_task_id.h"

#include <memory>

#include "content/public/browser/navigation_entry.h"

namespace sessions {
const char kTaskIdKey[] = "task_id_data";

NavigationTaskId::NavigationTaskId() {}

NavigationTaskId::NavigationTaskId(const NavigationTaskId& navigation_task_id) =
    default;

NavigationTaskId::~NavigationTaskId() {}

NavigationTaskId* NavigationTaskId::Get(content::NavigationEntry* entry) {
  NavigationTaskId* navigation_task_id =
      static_cast<NavigationTaskId*>(entry->GetUserData(kTaskIdKey));
  if (navigation_task_id)
    return navigation_task_id;
  auto navigation_task_id_ptr = std::make_unique<NavigationTaskId>();
  navigation_task_id = navigation_task_id_ptr.get();
  entry->SetUserData(kTaskIdKey, std::move(navigation_task_id_ptr));
  return navigation_task_id;
}

std::unique_ptr<base::SupportsUserData::Data> NavigationTaskId::Clone() {
  return std::make_unique<NavigationTaskId>(*this);
}

}  // namespace sessions
