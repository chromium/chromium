// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_TEST_SUPPORT_TEST_UTILS_H_
#define COMPONENTS_DATA_SHARING_TEST_SUPPORT_TEST_UTILS_H_

#include "components/data_sharing/public/group_data.h"
#include "url/gurl.h"

namespace data_sharing {

// Returns a data sharing link for a group token.
GURL GetDataSharingUrl(const GroupToken& group_token);

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_TEST_SUPPORT_TEST_UTILS_H_
