// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_TEST_UTILS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_TEST_UTILS_H_

#include <stddef.h>

namespace base {

class FilePath;

}  // namespace base

namespace offline_pages {

// The test_utils namespace within offline_pages namespace contains helper
// methods that are common and shared among all Offline Pages tests.
namespace test_utils {

// Get number of files in the given |dir|.
size_t GetFileCountInDirectory(const base::FilePath& directory);

}  // namespace test_utils

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_TEST_UTILS_H_
