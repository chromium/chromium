// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_CLASSIFICATIONS_LOADER_H_
#define CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_CLASSIFICATIONS_LOADER_H_

#include <unordered_map>

#include "base/no_destructor.h"
#include "chrome/browser/url_param_filter/url_param_filter_classification.pb.h"

namespace url_param_filter {

using ClassificationMap =
    std::unordered_map<std::string, url_param_filter::FilterClassification>;

class ClassificationsLoader {
 public:
  static ClassificationsLoader* GetInstance();

  ClassificationsLoader(const ClassificationsLoader&) = delete;
  ClassificationsLoader& operator=(const ClassificationsLoader&) = delete;

  const ClassificationMap& GetSourceClassifications();
  const ClassificationMap& GetDestinationClassifications();

 private:
  friend class base::NoDestructor<ClassificationsLoader>;

  ClassificationsLoader();
  ~ClassificationsLoader();
};

}  // namespace url_param_filter
#endif  // CHROME_BROWSER_URL_PARAM_FILTER_URL_PARAM_CLASSIFICATIONS_LOADER_H_
