// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/url_param_filter/url_param_classifications_loader.h"

#include "base/base64.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "chrome/browser/url_param_filter/url_param_filter_classification.pb.h"
#include "chrome/common/chrome_features.h"
#include "third_party/zlib/google/compression_utils.h"

namespace url_param_filter {

namespace {

ClassificationMap GetClassifications(FilterClassification_SiteRole role) {
  base::FieldTrialParams params;
  base::GetFieldTrialParamsByFeature(features::kIncognitoParamFilterEnabled,
                                     &params);
  auto classification_arg = params.find("classifications");
  FilterClassifications classifications;
  ClassificationMap map;
  if (classification_arg != params.end()) {
    std::string out;
    base::Base64Decode(classification_arg->second, &out);
    std::string uncompressed;
    if (compression::GzipUncompress(out, &uncompressed)) {
      if (classifications.ParseFromString(uncompressed)) {
        for (auto i : classifications.classifications()) {
          if (i.site_role() == role) {
            map[i.site()] = i;
          }
        }
      }
    }
  }
  return map;
}

}  // anonymous namespace

const ClassificationMap& ClassificationsLoader::GetSourceClassifications() {
  static const base::NoDestructor<ClassificationMap> source_classifications(
      GetClassifications(
          FilterClassification_SiteRole::FilterClassification_SiteRole_SOURCE));
  return *source_classifications;
}
const ClassificationMap&
ClassificationsLoader::GetDestinationClassifications() {
  static const base::NoDestructor<ClassificationMap>
      destination_classifications(
          GetClassifications(FilterClassification_SiteRole::
                                 FilterClassification_SiteRole_DESTINATION));
  return *destination_classifications;
}

// static
ClassificationsLoader* ClassificationsLoader::GetInstance() {
  static base::NoDestructor<ClassificationsLoader> instance;
  return instance.get();
}

ClassificationsLoader::ClassificationsLoader() = default;
ClassificationsLoader::~ClassificationsLoader() = default;

}  // namespace url_param_filter
