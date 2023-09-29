// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTINUOUS_SEARCH_RENDERER_CONFIG_H_
#define COMPONENTS_CONTINUOUS_SEARCH_RENDERER_CONFIG_H_

#include <string>

#include "base/feature_list.h"

namespace continuous_search {

BASE_DECLARE_FEATURE(kRelatedSearchesExtraction);

// Config for the search results extractor.
struct Config {
  // The ID of the related searches container.
#if BUILDFLAG(IS_ANDROID)
  std::string related_searches_id = "rso";
#else
  std::string related_searches_id = "bres";
#endif

// The classname of the related searches anchor element.
#if BUILDFLAG(IS_ANDROID)
  std::string related_searches_anchor_classname = "h9P1Xd";
#else
  std::string related_searches_anchor_classname = "k8XOCe";
#endif

// The classname of the related searches title element.
#if BUILDFLAG(IS_ANDROID)
  std::string related_searches_title_classname = "kTSm7b";
#else
  std::string related_searches_title_classname = "s75CSd";
#endif

  Config();
  Config(const Config& other);
  ~Config();
};

// Gets the current configuration.
const Config& GetConfig();

// Overrides the config returned by |GetConfig()|.
void SetConfigForTesting(const Config& config);

}  // namespace continuous_search

#endif  // COMPONENTS_CONTINUOUS_SEARCH_RENDERER_CONFIG_H_