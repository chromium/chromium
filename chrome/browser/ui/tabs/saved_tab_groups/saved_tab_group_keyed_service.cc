// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

SavedTabGroupKeyedService::SavedTabGroupKeyedService(Profile* profile)
    : model_(profile), listener_(&model_), profile_(profile) {}

SavedTabGroupKeyedService::~SavedTabGroupKeyedService() = default;
