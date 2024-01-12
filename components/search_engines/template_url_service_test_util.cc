// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_service_test_util.h"

#include <memory>

#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"

void RegisterPrefsForTemplateURLService(
    user_prefs::PrefRegistrySyncable* registry) {
  TemplateURLService::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  DefaultSearchManager::RegisterProfilePrefs(registry);
}
