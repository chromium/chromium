// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_HISTORY_TEST_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_HISTORY_TEST_UTIL_H_

#include "base/containers/span.h"

namespace history {

class HistoryService;
class URLRow;

void AddFakeURLsToHistoryService(HistoryService* history_service,
                                 base::span<const URLRow> url_rows);

}  // namespace history

#endif  // COMPONENTS_OMNIBOX_BROWSER_HISTORY_TEST_UTIL_H_
