// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_KEYED_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

class SavedTabGroupKeyedService : public KeyedService {
 public:
  explicit SavedTabGroupKeyedService(Profile* profile);
  SavedTabGroupKeyedService(const SavedTabGroupKeyedService&) = delete;
  SavedTabGroupKeyedService& operator=(const SavedTabGroupKeyedService& other) =
      delete;
  ~SavedTabGroupKeyedService() override;

  SavedTabGroupModelListener* listener() { return &listener_; }
  SavedTabGroupModel* model() { return &model_; }
  Profile* profile() { return profile_; }

 private:
  SavedTabGroupModel model_;
  SavedTabGroupModelListener listener_;
  raw_ptr<Profile> profile_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_KEYED_SERVICE_H_
