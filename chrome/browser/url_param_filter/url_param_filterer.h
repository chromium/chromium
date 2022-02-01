// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTERER_H_
#define CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTERER_H_

#include <unordered_map>
#include "chrome/browser/url_param_filter/url_param_filter_classification.pb.h"
#include "url/gurl.h"

namespace url_param_filter {
using ClassificationMap =
    std::unordered_map<std::string, url_param_filter::FilterClassification>;

// Filter the destination URL according to the parameter classifications for the
// source and destination URLs.
GURL FilterUrl(const GURL& source_url,
               const GURL& destination_url,
               const ClassificationMap& source_classification_map,
               const ClassificationMap& destination_classification_map);

}  // namespace url_param_filter
#endif  // CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_FILTERER_H_
