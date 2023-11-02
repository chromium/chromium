// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_test_helper.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_entry_restore_context.h"
#include "content/public/common/referrer.h"
#include "third_party/blink/public/common/page_state/page_state.h"

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

  // Initialize the NavigationEntry with a dummy PageState with unique
  // item and document sequence numbers. The item sequence number in particular
  // is important to initialize because it always defaults to the same value,
  // and it is checked during restore to find FrameNavigationEntries that can
  // be de-duplicated.
  static int64_t next_sequence_number = 1;
  int64_t item_sequence_number = next_sequence_number++;
  int64_t document_sequence_number = next_sequence_number++;
  std::unique_ptr<content::NavigationEntryRestoreContext> restore_context =
      content::NavigationEntryRestoreContext::Create();
  navigation_entry->SetPageState(
      blink::PageState::CreateForTestingWithSequenceNumbers(
          GURL(virtual_url), item_sequence_number, document_sequence_number),
      restore_context.get());
  return ContentSerializedNavigationBuilder::FromNavigationEntry(
      test_data::kIndex, navigation_entry.get());
}

}  // namespace sessions
