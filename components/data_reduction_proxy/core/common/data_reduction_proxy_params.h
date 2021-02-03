// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PARAMS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PARAMS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

// The data_reduction_proxy::params namespace is a collection of methods to
// determine the operating parameters of the Data Reduction Proxy as specified
// by field trials and command line switches.
namespace params {

// Returns true if this client is part of a field trial that should display
// a promotion for the data reduction proxy.
bool IsIncludedInPromoFieldTrial();

// Returns true if this client is part of a field trial that should display
// a FRE promotion for the data reduction proxy.
bool IsIncludedInFREPromoFieldTrial();

// Returns the server experiments option name. This name is used in the request
// headers to the data saver proxy. This name is also used to set the experiment
// name using finch trial.
std::string GetDataSaverServerExperimentsOptionName();

// Returns the server experiment. This name is used in the request
// headers to the data saver proxy. Returned value may be empty indicating no
// experiment is enabled.
std::string GetDataSaverServerExperiments();

}  // namespace params

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PARAMS_H_
