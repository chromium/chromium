// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_CLIENT_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_CLIENT_H_

#include <string>

#include "components/search_engines/template_url_id.h"

class GURL;
class TemplateURLService;

// This interface provides history related functionality required by
// TemplateURLService.
// TODO(hashimoto): Get rid of this once HistoryService gets componentized.
class TemplateURLServiceClient {
 public:
  virtual ~TemplateURLServiceClient() = default;

  // Called by TemplateURLService::Shutdown as part of the two phase shutdown
  // of the KeyedService.
  virtual void Shutdown() = 0;

  // Sets the pointer to the owner of this object.
  virtual void SetOwner(TemplateURLService* owner) = 0;

  // Deletes all search terms for the specified keyword.
  virtual void DeleteAllSearchTermsForKeyword(TemplateURLID id) = 0;

  // Sets the search terms for the specified url and keyword.
  virtual void SetKeywordSearchTermsForURL(const GURL& url,
                                           TemplateURLID id,
                                           const std::u16string& term) = 0;

  // Adds the given URL to history as a keyword generated visit.
  virtual void AddKeywordGeneratedVisit(const GURL& url) = 0;
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_CLIENT_H_
