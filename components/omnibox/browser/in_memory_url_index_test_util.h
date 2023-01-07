// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_IN_MEMORY_URL_INDEX_TEST_UTIL_H_
#define COMPONENTS_OMNIBOX_BROWSER_IN_MEMORY_URL_INDEX_TEST_UTIL_H_

class InMemoryURLIndex;

// If the given |index| is in the process of restoring, blocks until the restore
// is complete.
void BlockUntilInMemoryURLIndexIsRefreshed(InMemoryURLIndex* index);

#endif  // COMPONENTS_OMNIBOX_BROWSER_IN_MEMORY_URL_INDEX_TEST_UTIL_H_
