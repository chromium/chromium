// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"

SavedTabGroupKeyedService::SavedTabGroupKeyedService(Profile* profile)
    : profile_(profile), listener_(model(), profile) {}

SavedTabGroupKeyedService::~SavedTabGroupKeyedService() = default;
