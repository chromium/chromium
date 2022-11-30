// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_test_base.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "url/gurl.h"

InstantTestBase::InstantTestBase()
    : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
  https_test_server_.ServeFilesFromSourceDirectory("chrome/test/data");
}

InstantTestBase::~InstantTestBase() = default;

void InstantTestBase::SetupInstant(Profile* profile,
                                   const GURL& base_url,
                                   const GURL& ntp_url) {
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  search_test_utils::WaitForTemplateURLServiceToLoad(service);

  TemplateURLData data;
  data.SetShortName(u"name");
  data.SetURL(base_url.spec() + "q={searchTerms}&is_search");
  data.new_tab_url = ntp_url.spec();
  data.alternate_urls.push_back(base_url.spec() + "#q={searchTerms}");

  TemplateURL* template_url = service->Add(std::make_unique<TemplateURL>(data));
  service->SetUserSelectedDefaultSearchProvider(template_url);
}
