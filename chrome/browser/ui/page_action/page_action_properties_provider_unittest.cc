// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_action/page_action_properties_provider.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_variants_reader.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/action_id.h"

namespace page_actions {

namespace {

constexpr char kVariantsLocation[] =
    "//tools/metrics/histograms/metadata/page/histograms.xml:PageActionType";

// The PageActionController.{PageActionType}.* histograms are named from
// PageActionProperties::histogram_name, and the dashboard only knows the names
// listed as PageActionType variants in page/histograms.xml. Check that the two
// stay in sync in both directions, so that adding a page action without a
// variant, or removing one without deleting its variant, fails here instead of
// silently producing undocumented or dead histograms.
TEST(PageActionPropertiesProviderTest, HistogramNamesMatchVariants) {
  std::optional<base::HistogramVariantsEntryMap> variants;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    variants = base::ReadVariantsFromHistogramsXml("PageActionType", "page");
  }
  ASSERT_TRUE(variants.has_value());

  PageActionPropertiesProvider provider;
  std::set<std::string> histogram_names;
  for (actions::ActionId action_id : kActionIds) {
    ASSERT_TRUE(provider.Contains(action_id))
        << "ActionId " << static_cast<int>(action_id)
        << " is in kActionIds but has no entry in kPageActionProperties.";
    const char* histogram_name =
        provider.GetProperties(action_id).histogram_name;
    ASSERT_NE(histogram_name, nullptr)
        << "ActionId " << static_cast<int>(action_id)
        << " has no histogram_name.";
    EXPECT_TRUE(histogram_names.insert(histogram_name).second)
        << "histogram_name \"" << histogram_name
        << "\" is used by more than one page action, so their metrics are "
           "recorded together.";
  }

  std::vector<std::string> missing;
  for (const std::string& name : histogram_names) {
    if (!variants->contains(name)) {
      missing.push_back(name);
    }
  }
  EXPECT_TRUE(missing.empty())
      << "histogram_name(s) " << base::JoinString(missing, ", ")
      << " in page_action_properties_provider.cc have no <variant> in "
      << kVariantsLocation;

  std::vector<std::string> stale;
  for (const auto& [name, summary] : *variants) {
    if (!histogram_names.contains(name)) {
      stale.push_back(name);
    }
  }
  EXPECT_TRUE(stale.empty())
      << "<variant>(s) " << base::JoinString(stale, ", ") << " in "
      << kVariantsLocation
      << " are not the histogram_name of any page action; delete them.";
}

}  // namespace

}  // namespace page_actions
