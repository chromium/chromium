// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_

#include "components/sync_preferences/testing_pref_service_syncable.h"

void RegisterPrefsForTemplateURLService(
    user_prefs::PrefRegistrySyncable* registry);

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_
