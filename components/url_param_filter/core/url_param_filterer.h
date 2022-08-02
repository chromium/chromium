// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_PARAM_FILTER_CORE_URL_PARAM_FILTERER_H_
#define COMPONENTS_URL_PARAM_FILTER_CORE_URL_PARAM_FILTERER_H_

#include "components/url_param_filter/core/url_param_classifications_loader.h"
#include "components/url_param_filter/core/url_param_filter_classification.pb.h"
#include "url/gurl.h"

// Used to filter URL parameters based on backend classification rules. Note
// that all functions, unless otherwise specified, do not normalize the query
// string.
namespace url_param_filter {

namespace internal {

// Given a URL, get the label just to the left of the site's eTLD (e.g.
// subdomain.site.co.uk -> site).  Returns `absl::nullopt` for IP addresses,
// URLs that do not have hostnames, and other parsing errors.
absl::optional<std::string> GetLabelFromHostname(const GURL& gurl);

}  // namespace internal

// Represents the result of filtering; includes the resulting URL (which may be
// unmodified), along with the count of params filtered.
struct FilterResult {
  GURL filtered_url;
  int filtered_param_count;
  ClassificationExperimentStatus experimental_status;
};

// Filter the destination URL according to the parameter classifications for the
// source and destination URLs. Used internally by the 2-arg overload, and
// called directly from tests.
// Currently experimental; not intended for broad consumption.
FilterResult FilterUrl(const GURL& source_url,
                       const GURL& destination_url,
                       const ClassificationMap& classifications,
                       const FilterClassification::UseCase use_case);

// Filter the destination URL according to the default parameter classifications
// for the source and destination URLs. Equivalent to calling the three-arg
// version with a `use_case` of `UNKNOWN`. This overload is included for
// backward compatibility and will be removed.
// Currently experimental; not intended for broad consumption.
FilterResult FilterUrl(const GURL& source_url, const GURL& destination_url);

// Filter the destination URL according to the default parameter classifications
// for the source and destination URLs, only if the classifications include the
// passed-in `UseCase`.
// Currently experimental; not intended for broad consumption.
FilterResult FilterUrl(const GURL& source_url,
                       const GURL& destination_url,
                       const FilterClassification::UseCase use_case);

}  // namespace url_param_filter
#endif  // COMPONENTS_URL_PARAM_FILTER_CORE_URL_PARAM_FILTERER_H_
