// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_test_helper.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"

namespace sessions {

// static
SerializedNavigationEntry ContentTestHelper::CreateNavigation(
    const std::string& virtual_url,
    const std::string& title) {
  std::unique_ptr<content::NavigationEntry> navigation_entry =
      content::NavigationEntry::Create();
  navigation_entry->SetReferrer(
      content::Referrer(GURL("http://www.referrer.com"),
                        network::mojom::ReferrerPolicy::kDefault));
  navigation_entry->SetURL(GURL(virtual_url));
  navigation_entry->SetVirtualURL(GURL(virtual_url));
  navigation_entry->SetTitle(base::UTF8ToUTF16(title));
  navigation_entry->SetTimestamp(base::Time::Now());
  navigation_entry->SetHttpStatusCode(200);
  return ContentSerializedNavigationBuilder::FromNavigationEntry(
      test_data::kIndex, navigation_entry.get());
}

}  // namespace sessions
