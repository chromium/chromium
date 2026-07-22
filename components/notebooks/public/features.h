// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_PUBLIC_FEATURES_H_
#define COMPONENTS_NOTEBOOKS_PUBLIC_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace notebooks::features {

BASE_DECLARE_FEATURE(kNotebooks);
BASE_DECLARE_FEATURE_PARAM(std::string, kNotebookHomeURL);

// The base URL for the Notebooks API endpoints.
BASE_DECLARE_FEATURE_PARAM(std::string, kNotebooksApiBaseURL);

// URL path suffix for notebook source requests. Format is
// {kNotebooksApiBaseURL}/{notebook_id}/{kNotebookSourceURLSuffix}/{source_id}
BASE_DECLARE_FEATURE_PARAM(std::string, kNotebookSourceURLSuffix);

// Chrome Notebooks product identifier for backend logs.
BASE_DECLARE_FEATURE_PARAM(std::string, kProvenanceOriginProductId);

}  // namespace notebooks::features

#endif  // COMPONENTS_NOTEBOOKS_PUBLIC_FEATURES_H_
