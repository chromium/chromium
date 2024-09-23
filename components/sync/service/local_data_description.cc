// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/local_data_description.h"

#include <algorithm>
#include <ranges>
#include <set>
#include <utility>
#include <vector>

#include "base/i18n/message_formatter.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"

// Must come after all includes for JniType conversions.
#include "components/sync/android/jni_headers/LocalDataDescription_jni.h"

using base::android::ToJavaArrayOfStrings;
#endif

namespace syncer {

LocalDataDescription::LocalDataDescription() = default;

LocalDataDescription::LocalDataDescription(const std::vector<GURL>& all_urls)
    : item_count(all_urls.size()) {
  // Using a set to get only the distinct domains. This also ensures an
  // alphabetical ordering of the domains.
  std::set<std::string> domain_set;
  std::ranges::transform(
      all_urls, std::inserter(domain_set, domain_set.end()),
      [](const GURL& url) {
        return base::UTF16ToUTF8(
            url_formatter::
                FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(url));
      });
  domain_count = domain_set.size();
  // Add up to 3 domains as examples to be used in a string shown to the user.
  std::ranges::copy_n(domain_set.begin(),
                      std::min(size_t{3}, domain_set.size()),
                      std::back_inserter(domains));
}

LocalDataDescription::LocalDataDescription(const LocalDataDescription&) =
    default;

LocalDataDescription& LocalDataDescription::operator=(
    const LocalDataDescription&) = default;

LocalDataDescription::LocalDataDescription(LocalDataDescription&&) = default;

LocalDataDescription& LocalDataDescription::operator=(LocalDataDescription&&) =
    default;

LocalDataDescription::~LocalDataDescription() = default;

std::u16string GetDomainsDisplayText(const LocalDataDescription& description) {
  CHECK_GT(description.domains.size(), 0u);
  if (description.domains.size() == 1) {
    return base::i18n::MessageFormatter::FormatWithNamedArgs(
        l10n_util::GetStringUTF16(IDS_BULK_UPLOAD_SUBTITLE), "count",
        static_cast<int>(description.domain_count), "website_1",
        description.domains[0], "more_count",
        static_cast<int>(description.domain_count - 1));
  }
  return base::i18n::MessageFormatter::FormatWithNamedArgs(
      l10n_util::GetStringUTF16(IDS_BULK_UPLOAD_SUBTITLE), "count",
      static_cast<int>(description.domain_count), "website_1",
      description.domains[0], "website_2", description.domains[1], "more_count",
      static_cast<int>(description.domain_count - 2));
}

void PrintTo(const LocalDataDescription& desc, std::ostream* os) {
  *os << "{ item_count:" << desc.item_count << ", domains:[";
  for (const auto& domain : desc.domains) {
    *os << domain << ",";
  }
  *os << "], domain_count:" << desc.domain_count;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaLocalDataDescription(
    JNIEnv* env,
    const LocalDataDescription& local_data_description) {
  return Java_LocalDataDescription_Constructor(
      env, local_data_description.item_count,
      base::android::ToJavaArrayOfStrings(env, local_data_description.domains),
      local_data_description.domain_count);
}

std::u16string JNI_LocalDataDescription_GetDomainsDisplayText(
    JNIEnv* env,
    int item_count,
    std::vector<std::string>& domains,
    int domain_count) {
  LocalDataDescription description;
  description.item_count = item_count;
  description.domains = std::move(domains);
  description.domain_count = domain_count;
  return GetDomainsDisplayText(std::move(description));
}
#endif

}  // namespace syncer
