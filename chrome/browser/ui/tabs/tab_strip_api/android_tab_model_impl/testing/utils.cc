// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

namespace tabs_api::testing {

TabModel& GetTabModel(Profile* profile) {
  for (TabModel* model : TabModelList::models()) {
    if (model->GetProfile() == profile) {
      return *model;
    }
  }
  NOTREACHED() << "could not find a tab model to construct the api with";
}

}  // namespace tabs_api::testing
