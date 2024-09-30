// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_DATA_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/template_url_id.h"
#include "url/gurl.h"

// The data for the TemplateURL.  Separating this into its own class allows most
// users to do SSA-style usage of TemplateURL: construct a TemplateURLData with
// whatever fields are desired, then create an immutable TemplateURL from it.
struct TemplateURLData {
  enum class CreatedByPolicy {
    kNoPolicy = 0,
    kDefaultSearchProvider = 1,
    kSiteSearch = 2,
  };

  using RegulatoryExtension = TemplateURLPrepopulateData::RegulatoryExtension;

  TemplateURLData();
  TemplateURLData(const TemplateURLData& other);
  TemplateURLData& operator=(const TemplateURLData& other);

  // Creates a TemplateURLData suitable for prepopulated engines.
  // Note that unlike in the default constructor, |safe_for_autoreplace| will
  // be set to true. date_created and last_modified will be set to null time
  // value, instead of current time.
  // std::string_view in arguments is used to pass const char* pointer members
  // of PrepopulatedEngine structure which can be nullptr.
  TemplateURLData(std::u16string_view name,
                  std::u16string_view keyword,
                  std::string_view search_url,
                  std::string_view suggest_url,
                  std::string_view image_url,
                  std::string_view image_translate_url,
                  std::string_view new_tab_url,
                  std::string_view contextual_search_url,
                  std::string_view logo_url,
                  std::string_view doodle_url,
                  std::string_view search_url_post_params,
                  std::string_view suggest_url_post_params,
                  std::string_view image_url_post_params,
                  std::string_view side_search_param,
                  std::string_view side_image_search_param,
                  std::string_view image_translate_source_language_param_key,
                  std::string_view image_translate_target_language_param_key,
                  std::vector<std::string> search_intent_params,
                  std::string_view favicon_url,
                  std::string_view encoding,
                  std::u16string_view image_search_branding_label,
                  const base::Value::List& alternate_urls_list,
                  bool preconnect_to_search_url,
                  bool prefetch_likely_navigations,
                  int prepopulate_id,
                  const base::span<const RegulatoryExtension>& extensions);

  ~TemplateURLData();

  // A short description of the template. This is the name we show to the user
  // in various places that use TemplateURLs. For example, the location bar
  // shows this when the user selects a substituting match.
  void SetShortName(std::u16string_view short_name);
  const std::u16string& short_name() const { return short_name_; }

  // The shortcut for this TemplateURL.  |keyword| must be non-empty.
  void SetKeyword(std::u16string_view keyword);
  const std::u16string& keyword() const { return keyword_; }

  // The raw URL for the TemplateURL, which may not be valid as-is (e.g. because
  // it requires substitutions first).  This must be non-empty.
  void SetURL(const std::string& url);
  const std::string& url() const { return url_; }

  // Recomputes |sync_guid| using the same logic as in the constructor. This
  // means a random GUID is generated, except for built-in search engines,
  // which generate GUIDs deterministically based on |prepopulate_id| or
  // |starter_pack_id|.
  void GenerateSyncGUID();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Optional additional raw URLs.
  std::string suggestions_url;
  std::string image_url;
  std::string image_translate_url;
  std::string new_tab_url;
  std::string contextual_search_url;

  // Optional URL for the logo.
  GURL logo_url;

  // Optional URL for the Doodle.
  GURL doodle_url;

  // The following post_params are comma-separated lists used to specify the
  // post parameters for the corresponding URL.
  std::string search_url_post_params;
  std::string suggestions_url_post_params;
  std::string image_url_post_params;

  // The parameter appended to the engine's search URL when constructing the URL
  // for the side search side panel.
  std::string side_search_param;

  // The parameter appended to the engine's image URL when constructing the
  // URL for the image search entry in the side panel.
  std::string side_image_search_param;

  // The key of the parameter identifying the source language for an image
  // translation.
  std::string image_translate_source_language_param_key;

  // The key of the parameter identifying the target language for an image
  // translation.
  std::string image_translate_target_language_param_key;

  // Brand name used for image search queries. If not set, the short_name
  // will be used.
  std::u16string image_search_branding_label;

  // The parameters making up the engine's canonical search URL in addition to
  // the search terms. These params disambiguate the search terms and determine
  // the fulfillment.
  std::vector<std::string> search_intent_params;

  // Favicon for the TemplateURL.
  GURL favicon_url;

  // URL to the OSD file this came from. May be empty.
  GURL originating_url;

  // Whether it's safe for auto-modification code (the autogenerator and the
  // code that imports data from other browsers) to replace the TemplateURL.
  // This should be set to false for any TemplateURL the user edits, or any
  // TemplateURL that the user clearly manually edited in the past, like a
  // bookmark keyword from another browser.
  bool safe_for_autoreplace;

  // The list of supported encodings for the search terms. This may be empty,
  // which indicates the terms should be encoded with UTF-8.
  std::vector<std::string> input_encodings;

  // Unique identifier of this TemplateURL. The unique ID is set by the
  // TemplateURLService when the TemplateURL is added to it.
  TemplateURLID id;

  // Date this TemplateURL was created.
  //
  // NOTE: this may be 0, which indicates the TemplateURL was created before we
  // started tracking creation time.
  base::Time date_created;

  // The last time this TemplateURL was modified by a user, since creation.
  //
  // NOTE: Like date_created above, this may be 0.
  base::Time last_modified;

  // Date when this TemplateURL was last visited.
  //
  // NOTE: This might be 0 if the TemplateURL has never been visited.
  base::Time last_visited;

  // True if this TemplateURL was automatically created by the administrator via
  // group policy.
  CreatedByPolicy created_by_policy;

  // True if this TemplateURL is forced to be the default search engine via
  // policy. This prevents the user from setting another search engine as
  // default.
  // False if this TemplateURL is recommended or not set via policy. This allows
  // the user to set another search engine as default.
  bool enforced_by_policy;

  // True if this TemplateURL was created from metadata received from Play API.
  bool created_from_play_api;

  // True if this TemplateURL should be promoted in the Omnibox along with the
  // starter pack.
  bool featured_by_policy = false;

  // Number of times this TemplateURL has been explicitly used to load a URL.
  // We don't increment this for uses as the "default search engine" since
  // that's not really "explicit" usage and incrementing would result in pinning
  // the user's default search engine(s) to the top of the list of searches on
  // the New Tab page, de-emphasizing the omnibox as "where you go to search".
  int usage_count;

  // If this TemplateURL comes from prepopulated data the prepopulate_id is > 0.
  int prepopulate_id;

  // The primary unique identifier for Sync. This set on all TemplateURLs
  // regardless of whether they have been associated with Sync.
  std::string sync_guid;

  // A list of URL patterns that can be used, in addition to |url_|, to extract
  // search terms from a URL.
  std::vector<std::string> alternate_urls;

  // A list of regulatory extensions, keyed by extension variant.
  base::flat_map<RegulatoryExtensionType,
                 raw_ptr<const RegulatoryExtension, CtnExperimental>>
      regulatory_extensions;

  // Whether a connection to |url_| should regularly be established when this is
  // set as the "default search engine".
  bool preconnect_to_search_url = false;

  // Whether the client is allowed to prefetch Search queries that are likely
  // (in addition to queries that are recommended via suggestion server). This
  // is experimental.
  bool prefetch_likely_navigations = false;

  enum class ActiveStatus {
    kUnspecified = 0,  // The default value when a search engine is auto-added.
    kTrue,             // Search engine is active.
    kFalse,            // SE has been manually deactivated by a user.
  };

  // Whether this entry is "active". Active entries can be invoked by keyword
  // via the omnibox.  Inactive search engines do nothing until they have been
  // activated.  A search engine is inactive if it's unspecified or false.
  ActiveStatus is_active{ActiveStatus::kUnspecified};

  // This TemplateURL is part of the built-in "starter pack" if
  // starter_pack_id > 0.
  int starter_pack_id{0};

 private:
  // Private so we can enforce using the setters and thus enforce that these
  // fields are never empty.
  std::u16string short_name_;
  std::u16string keyword_;
  std::string url_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_DATA_H_
