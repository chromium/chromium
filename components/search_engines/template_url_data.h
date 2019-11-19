// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_DATA_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_DATA_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/search_engines/template_url_id.h"
#include "url/gurl.h"

namespace base {
class ListValue;
}

// The data for the TemplateURL.  Separating this into its own class allows most
// users to do SSA-style usage of TemplateURL: construct a TemplateURLData with
// whatever fields are desired, then create an immutable TemplateURL from it.
struct TemplateURLData {
  TemplateURLData();
  TemplateURLData(const TemplateURLData& other);
  TemplateURLData& operator=(const TemplateURLData& other);

  // Creates a TemplateURLData suitable for prepopulated engines.
  // Note that unlike in the default constructor, |safe_for_autoreplace| will
  // be set to true. date_created and last_modified will be set to null time
  // value, instead of current time.
  // StringPiece in arguments is used to pass const char* pointer members
  // of PrepopulatedEngine structure which can be nullptr.
  TemplateURLData(const base::string16& name,
                  const base::string16& keyword,
                  base::StringPiece search_url,
                  base::StringPiece suggest_url,
                  base::StringPiece image_url,
                  base::StringPiece new_tab_url,
                  base::StringPiece contextual_search_url,
                  base::StringPiece logo_url,
                  base::StringPiece doodle_url,
                  base::StringPiece search_url_post_params,
                  base::StringPiece suggest_url_post_params,
                  base::StringPiece image_url_post_params,
                  base::StringPiece favicon_url,
                  base::StringPiece encoding,
                  const base::ListValue& alternate_urls_list,
                  int prepopulate_id);

  ~TemplateURLData();

  // A short description of the template. This is the name we show to the user
  // in various places that use TemplateURLs. For example, the location bar
  // shows this when the user selects a substituting match.
  void SetShortName(const base::string16& short_name);
  const base::string16& short_name() const { return short_name_; }

  // The shortcut for this TemplateURL.  |keyword| must be non-empty.
  void SetKeyword(const base::string16& keyword);
  const base::string16& keyword() const { return keyword_; }

  // The raw URL for the TemplateURL, which may not be valid as-is (e.g. because
  // it requires substitutions first).  This must be non-empty.
  void SetURL(const std::string& url);
  const std::string& url() const { return url_; }

  // Recomputes |sync_guid| using the same logic as in the constructor. This
  // means a random GUID is generated, except for prepopulated search engines,
  // which generate GUIDs deterministically based on |prepopulate_id|.
  void GenerateSyncGUID();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Optional additional raw URLs.
  std::string suggestions_url;
  std::string image_url;
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
  bool created_by_policy;

  // True if this TemplateURL was created from metadata received from Play API.
  bool created_from_play_api;

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

 private:
  // Private so we can enforce using the setters and thus enforce that these
  // fields are never empty.
  base::string16 short_name_;
  base::string16 keyword_;
  std::string url_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_DATA_H_
