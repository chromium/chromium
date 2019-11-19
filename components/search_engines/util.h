// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_UTIL_H_
#define COMPONENTS_SEARCH_ENGINES_UTIL_H_

// This file contains utility functions for search engine functionality.

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/search_engines/template_url_service.h"

class KeywordWebDataService;
class PrefService;
class TemplateURL;
class WDTypedResult;

// Returns the short name of the default search engine, or the empty string if
// none is set.
base::string16 GetDefaultSearchEngineName(TemplateURLService* service);

// Returns a GURL that searches for |terms| using the default search engine of
// |service|.
GURL GetDefaultSearchURLForSearchTerms(TemplateURLService* service,
                                       const base::string16& terms);

// Returns matching URL from |template_urls| or NULL.
TemplateURL* FindURLByPrepopulateID(
    const TemplateURLService::TemplateURLVector& template_urls,
    int prepopulate_id);

// Modifies |prepopulated_url| so that it contains user-modified fields from
// |original_turl|. Both URLs must have the same prepopulate_id.
void MergeIntoPrepopulatedEngineData(const TemplateURL* original_turl,
                                     TemplateURLData* prepopulated_url);

// CreateActionsFromCurrentPrepopulateData() (see below) takes in the current
// prepopulated URLs as well as the user's current URLs, and returns an instance
// of the following struct representing the changes necessary to bring the
// user's URLs in line with the prepopulated URLs.
//
// There are three types of changes:
// (1) Previous prepopulated engines that no longer exist in the current set of
//     prepopulated engines and thus should be removed from the user's current
//     URLs.
// (2) Previous prepopulated engines whose data has changed.  The existing
//     entries for these engines should be updated to reflect the new data,
//     except for any user-set names and keywords, which can be preserved.
// (3) New prepopulated engines not in the user's engine list, which should be
//     added.

// The pair of current search engine and its new value.
typedef std::pair<TemplateURL*, TemplateURLData> EditedSearchEngine;
typedef std::vector<EditedSearchEngine> EditedEngines;

struct ActionsFromPrepopulateData {
  ActionsFromPrepopulateData();
  ActionsFromPrepopulateData(const ActionsFromPrepopulateData& other);
  ~ActionsFromPrepopulateData();

  TemplateURLService::TemplateURLVector removed_engines;
  EditedEngines edited_engines;
  std::vector<TemplateURLData> added_engines;
};

// MergeEnginesFromPrepopulateData merges search engines from
// |prepopulated_urls| into |template_urls|. Calls
// CreateActionsFromCurrentPrepopulateData() to collect actions and then applies
// them on |tempate_urls|. MergeEnginesFromPrepopulateData is invoked when the
// version of the prepopulate data changes. If |removed_keyword_guids| is not
// nullptr, the Sync GUID of each item removed from the DB will be added to it.
// Note that this function will take ownership of |prepopulated_urls| and will
// clear the vector.
// The function is exposed in header file to provide access from unittests.
void MergeEnginesFromPrepopulateData(
    KeywordWebDataService* service,
    std::vector<std::unique_ptr<TemplateURLData>>* prepopulated_urls,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    std::set<std::string>* removed_keyword_guids);

// Given the user's current URLs and the current set of prepopulated URLs,
// produces the set of actions (see above) required to make the user's URLs
// reflect the prepopulate data.  |default_search_provider| is used to avoid
// placing the current default provider on the "to be removed" list.
//
// NOTE: Takes ownership of, and clears, |prepopulated_urls|.
ActionsFromPrepopulateData CreateActionsFromCurrentPrepopulateData(
    std::vector<std::unique_ptr<TemplateURLData>>* prepopulated_urls,
    const TemplateURLService::OwnedTemplateURLVector& existing_urls,
    const TemplateURL* default_search_provider);

// Processes the results of KeywordWebDataService::GetKeywords, combining it
// with prepopulated search providers to result in:
//  * a set of template_urls (search providers). The caller owns the
//    TemplateURL* returned in template_urls.
//  * whether there is a new resource keyword version (and the value).
//    |*new_resource_keyword_version| is set to 0 if no new value. Otherwise,
//    it is the new value.
// Only pass in a non-NULL value for service if the KeywordWebDataService should
// be updated. If |removed_keyword_guids| is not NULL, any TemplateURLs removed
// from the keyword table in the KeywordWebDataService will have their Sync
// GUIDs added to it. |default_search_provider| will be used to prevent removing
// the current user-selected DSE, regardless of changes in prepopulate data.
void GetSearchProvidersUsingKeywordResult(
    const WDTypedResult& result,
    KeywordWebDataService* service,
    PrefService* prefs,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    int* new_resource_keyword_version,
    std::set<std::string>* removed_keyword_guids);

// Like GetSearchProvidersUsingKeywordResult(), but allows the caller to pass in
// engines in |template_urls| instead of getting them via processing a web data
// service request.
// |resource_keyword_version| should contain the version number of the current
// keyword data, i.e. the version number of the most recent prepopulate data
// that has been merged into the current keyword data.  On exit, this will be
// set as in GetSearchProvidersUsingKeywordResult().
void GetSearchProvidersUsingLoadedEngines(
    KeywordWebDataService* service,
    PrefService* prefs,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data,
    int* resource_keyword_version,
    std::set<std::string>* removed_keyword_guids);

// Due to a bug, the |input_encodings| field of TemplateURLData could have
// contained duplicate entries.  This removes those entries and returns whether
// any were found.
bool DeDupeEncodings(std::vector<std::string>* encodings);

// Removes (and deletes) TemplateURLs from |template_urls| and |service| if they
// have duplicate prepopulate ids. If |removed_keyword_guids| is not NULL, the
// Sync GUID of each item removed from the DB will be added to it. This is a
// helper used by GetSearchProvidersUsingKeywordResult(), but is declared here
// so it's accessible by unittests.
// The order of template_urls is preserved (except for duplicates) because it
// affects order of presentation in settings web-ui.
// See https://crbug.com/924268 for details.
void RemoveDuplicatePrepopulateIDs(
    KeywordWebDataService* service,
    const std::vector<std::unique_ptr<TemplateURLData>>& prepopulated_urls,
    TemplateURL* default_search_provider,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    const SearchTermsData& search_terms_data,
    std::set<std::string>* removed_keyword_guids);

TemplateURLService::OwnedTemplateURLVector::iterator FindTemplateURL(
    TemplateURLService::OwnedTemplateURLVector* urls,
    const TemplateURL* url);

#endif  // COMPONENTS_SEARCH_ENGINES_UTIL_H_
