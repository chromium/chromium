// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_UTIL_H_
#define COMPONENTS_SEARCH_ENGINES_UTIL_H_

// This file contains utility functions for search engine functionality.

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "components/search_engines/template_url_service.h"

class KeywordWebDataService;
class PrefService;
class TemplateURL;
class WDTypedResult;

// Returns the short name of the default search engine, or the empty string if
// none is set.
std::u16string GetDefaultSearchEngineName(TemplateURLService* service);

// Returns a GURL that searches for |terms| using the default search engine of
// |service|.
GURL GetDefaultSearchURLForSearchTerms(TemplateURLService* service,
                                       const std::u16string& terms);

// Returns matching URL from |template_urls| or NULL.
TemplateURL* FindURLByPrepopulateID(
    const TemplateURLService::TemplateURLVector& template_urls,
    int prepopulate_id);

enum class TemplateURLMergeOption {
  kDefault,
  kOverwriteUserEdits,
};

// Modifies `url_to_update` so that it contains user-modified fields from
// `original_turl`. Both URLs must have the same `prepopulate_id` or
// `starter_pack_id`. If `merge_option` is set to kOverWriteUserEdits,
// user-modified fields and `safe_for_autoreplace` are not preserved.
//
// WARNING: Changing merge_option from the default value will result in loss of
// user data. It should be set to kDefault unless in very specific circumstances
// where a reset to defaults is required.
void MergeIntoEngineData(
    const TemplateURL* original_turl,
    TemplateURLData* url_to_update,
    TemplateURLMergeOption merge_option = TemplateURLMergeOption::kDefault);

// CreateActionsFromCurrentPrepopulateData() and
// CreateActionsFromStarterPackData() (see below) takes in the current built-in
// (prepopulated or starter pack) URLs as well as the user's current URLs, and
// returns an instance of the following struct representing the changes
// necessary to bring the user's URLs in line with the built-in URLs.
//
// There are three types of changes:
// (1) Previous built-in engines that no longer exist in the current set of
//     built-in engines and thus should be removed from the user's current
//     URLs.
// (2) Previous built-in engines whose data has changed.  The existing
//     entries for these engines should be updated to reflect the new data,
//     except for any user-set names and keywords, which can be preserved.
// (3) New built-in engines not in the user's engine list, which should be
//     added.

// The pair of current search engine and its new value.
typedef std::pair<TemplateURL*, TemplateURLData> EditedSearchEngine;
typedef std::vector<EditedSearchEngine> EditedEngines;

struct ActionsFromCurrentData {
  ActionsFromCurrentData();
  ActionsFromCurrentData(const ActionsFromCurrentData& other);
  ~ActionsFromCurrentData();

  TemplateURLService::TemplateURLVector removed_engines;
  EditedEngines edited_engines;
  std::vector<TemplateURLData> added_engines;
};

// MergeEnginesFromPrepopulateData merges search engines from
// |prepopulated_urls| into |template_urls|. Calls
// CreateActionsFromCurrentPrepopulateData() to collect actions and then applies
// them on |template_urls|. MergeEnginesFromPrepopulateData is invoked when the
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
ActionsFromCurrentData CreateActionsFromCurrentPrepopulateData(
    std::vector<std::unique_ptr<TemplateURLData>>* prepopulated_urls,
    const TemplateURLService::OwnedTemplateURLVector& existing_urls,
    const TemplateURL* default_search_provider);

// MergeEnginesFromStarterPackData merges search engines from the built-in
// TemplateURLStarterPackData class into `template_urls`. Calls
// CreateActionsFromCurrentStarterPackData() to collect actions and then applies
// them on `template_urls`. MergeEgninesFromStarterPackData is invoked when the
// version of the starter pack data changes. If `removed_keyword_guids` is not
// nullptr, the Sync GUID of each item removed from the DB will be added to it.
// `merge_option` specifies whether user-modified fields are preserved when
// merging.  It should be set to default except for very specific use cases
// where a reset to defaults is required.
void MergeEnginesFromStarterPackData(
    KeywordWebDataService* service,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    std::set<std::string>* removed_keyword_guids,
    TemplateURLMergeOption merge_option = TemplateURLMergeOption::kDefault);

// Given the user's current URLs and the current set of Starter Pack URLs,
// produces the set of actions (see above) required to make the user's URLs
// reflect the starter pack data.
// `merge_option` specifies whether user-modified fields are preserved when
// merging.  It should be set to default except for very specific use cases
// where a reset to defaults is required.
//
// NOTE: Takes ownership of, and clears, |starter_pack_urls|.
ActionsFromCurrentData CreateActionsFromCurrentStarterPackData(
    std::vector<std::unique_ptr<TemplateURLData>>* starter_pack_urls,
    const TemplateURLService::OwnedTemplateURLVector& existing_urls,
    TemplateURLMergeOption merge_option = TemplateURLMergeOption::kDefault);

// Takes in an ActionsFromCurrentData (see above) and applies the actions (add,
// edit, or remove) to the user's current URLs.  This is called by
// MergeEnginesFromPrepopulateData() and MergeEnginesFromStarterPackData().
void ApplyActionsFromCurrentData(
    ActionsFromCurrentData actions,
    KeywordWebDataService* service,
    TemplateURLService::OwnedTemplateURLVector* template_urls,
    TemplateURL* default_search_provider,
    std::set<std::string>* removed_keyword_guids);

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
    int* new_resource_starter_pack_version,
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
    int* resource_starter_pack_version,
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
