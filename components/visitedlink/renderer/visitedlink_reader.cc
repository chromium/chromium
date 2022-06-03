// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visitedlink/renderer/visitedlink_reader.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/check.h"
#include "third_party/blink/public/web/web_view.h"

using blink::WebView;

namespace visitedlink {

VisitedLinkReader::VisitedLinkReader() = default;

VisitedLinkReader::~VisitedLinkReader() {
  FreeTable();
}

base::RepeatingCallback<
    void(mojo::PendingReceiver<mojom::VisitedLinkNotificationSink>)>
VisitedLinkReader::GetBindCallback() {
  return base::BindRepeating(&VisitedLinkReader::Bind,
                             weak_factory_.GetWeakPtr());
}

// Initializes the table with the given shared memory handle. This memory is
// mapped into the process.
void VisitedLinkReader::UpdateVisitedLinks(
    base::ReadOnlySharedMemoryRegion table_region) {
  // Since this function may be called again to change the table, we may need
  // to free old objects.
  FreeTable();
  DCHECK(hash_table_ == nullptr);

  int32_t table_len = 0;
  {
    // Map the header into our process so we can see how long the rest is,
    // and set the salt.
    base::ReadOnlySharedMemoryMapping header_mapping =
        table_region.MapAt(0, sizeof(SharedHeader));
    if (!header_mapping.IsValid())
      return;

    const SharedHeader* header =
        static_cast<const SharedHeader*>(header_mapping.memory());
    table_len = header->length;
    memcpy(salt_, header->salt, sizeof(salt_));
  }

  // Now we know the length, so map the table contents.
  table_mapping_ = table_region.Map();
  if (!table_mapping_.IsValid())
    return;

  // Commit the data.
  hash_table_ = const_cast<Fingerprint*>(reinterpret_cast<const Fingerprint*>(
      static_cast<const SharedHeader*>(table_mapping_.memory()) + 1));
  table_length_ = table_len;
}

void VisitedLinkReader::AddVisitedLinks(
    const std::vector<VisitedLinkReader::Fingerprint>& fingerprints) {
  for (size_t i = 0; i < fingerprints.size(); ++i)
    WebView::UpdateVisitedLinkState(fingerprints[i]);
}

void VisitedLinkReader::ResetVisitedLinks(bool invalidate_hashes) {
  WebView::ResetVisitedLinkState(invalidate_hashes);
}

void VisitedLinkReader::FreeTable() {
  if (!hash_table_)
    return;

  table_mapping_ = base::ReadOnlySharedMemoryMapping();
  hash_table_ = nullptr;
  table_length_ = 0;
}

void VisitedLinkReader::Bind(
    mojo::PendingReceiver<mojom::VisitedLinkNotificationSink> receiver) {
  receiver_.Bind(std::move(receiver));
}

}  // namespace visitedlink
