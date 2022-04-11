// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTERER_H_
#define CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTERER_H_

#include "chrome/browser/url_param_filter/url_param_classifications_loader.h"
#include "url/gurl.h"

// Used to filter URL parameters based on backend classification rules. Note
// that all functions, unless otherwise specified, do not normalize the query
// string.
namespace url_param_filter {

// Represents the result of filtering; includes the resulting URL (which may be
// unmodified), along with the count of params filtered.
struct FilterResult {
  GURL filtered_url;
  int filtered_param_count;
};

// Filter the destination URL according to the parameter classifications for the
// source and destination URLs. Used internally by the 2-arg overload, and
// called directly from tests.
// Currently experimental; not intended for broad consumption.
FilterResult FilterUrl(const GURL& source_url,
                       const GURL& destination_url,
                       const ClassificationMap& source_classification_map,
                       const ClassificationMap& destination_classification_map);

// Filter the destination URL according to the default parameter classifications
// for the source and destination URLs.
// Currently experimental; not intended for broad consumption.
FilterResult FilterUrl(const GURL& source_url, const GURL& destination_url);

}  // namespace url_param_filter
#endif  // CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTERER_H_
