// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_MODEL_UTILS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_MODEL_UTILS_H_

#include <string>


class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace offline_pages {

enum class OfflinePagesNamespaceEnumeration;

namespace model_utils {

// Return the enum value of the namespace represented by |name_space|.
OfflinePagesNamespaceEnumeration ToNamespaceEnum(const std::string& name_space);

// Metric collection related.
std::string AddHistogramSuffix(const std::string& name_space,
                               const char* histogram_name);

base::FilePath GenerateUniqueFilenameForOfflinePage(
    const std::u16string& title,
    const GURL& url,
    const base::FilePath& target_dir);

}  // namespace model_utils

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_OFFLINE_PAGE_MODEL_UTILS_H_
