// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_FEATURES_H_
#define COMPONENTS_BROWSING_DATA_CORE_FEATURES_H_

#include "base/feature_list.h"

namespace browsing_data {
namespace features {

// Enable removal of all third-party cookies and site data.
extern const base::Feature kEnableRemovingAllThirdPartyCookies;

}  // namespace features
}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_FEATURES_H_
