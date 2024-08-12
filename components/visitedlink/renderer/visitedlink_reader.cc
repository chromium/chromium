// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/visitedlink/renderer/visitedlink_reader.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"
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

uint64_t VisitedLinkReader::ComputePartitionedFingerprint(
    std::string_view canonical_link_url,
    const net::SchemefulSite& top_level_site,
    const url::Origin& frame_origin) {
  // Ensure that we can determine a valid triple-partition key for this link. If
  // invalid, we return the null fingerprint.
  const VisitedLink link = {GURL(canonical_link_url), top_level_site,
                            frame_origin};
  if (!link.IsValid()) {
    return 0;
  }
  // Determine the per-origin salt used for this fingerprint.
  auto it = salts_.find(frame_origin);
  if (it != salts_.end()) {
    return VisitedLinkCommon::ComputePartitionedFingerprint(link, it->second);
  }
  // If we cannot determine the per-origin salt, we cannot read the hashtable,
  // so we must return the null fingerprint.
  return 0;
}

// Initializes the table with the given shared memory handle. This memory is
// mapped into the process.
void VisitedLinkReader::UpdateVisitedLinks(
    base::ReadOnlySharedMemoryRegion table_region) {
  // Since this function may be called again to change the table, we may need
  // to free old objects.
  FreeTable();
  DCHECK(hash_table_ == nullptr);
  if (base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabase) ||
      base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks)) {
    return UpdatePartitionedVisitedLinks(std::move(table_region));
  }
  return UpdateUnpartitionedVisitedLinks(std::move(table_region));
}

void VisitedLinkReader::UpdateUnpartitionedVisitedLinks(
    base::ReadOnlySharedMemoryRegion table_region) {
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
  base::UmaHistogramCounts10M(
      "History.VisitedLinks.HashTableLengthOnReaderInit", table_length_);
}

void VisitedLinkReader::UpdatePartitionedVisitedLinks(
    base::ReadOnlySharedMemoryRegion table_region) {
  int32_t table_len = 0;
  {
    // Map the header into our process so we can see how long the rest is,
    // and set the salt.
    base::ReadOnlySharedMemoryMapping header_mapping =
        table_region.MapAt(0, sizeof(PartitionedSharedHeader));
    if (!header_mapping.IsValid()) {
      return;
    }

    const PartitionedSharedHeader* header =
        static_cast<const PartitionedSharedHeader*>(header_mapping.memory());
    table_len = header->length;
  }

  // Now we know the length, so map the table contents.
  table_mapping_ = table_region.Map();
  if (!table_mapping_.IsValid()) {
    return;
  }

  // Commit the data.
  hash_table_ = const_cast<Fingerprint*>(reinterpret_cast<const Fingerprint*>(
      static_cast<const PartitionedSharedHeader*>(table_mapping_.memory()) +
      1));
  table_length_ = table_len;
  base::UmaHistogramCounts10M(
      "History.VisitedLinks.HashTableLengthOnReaderInit", table_length_);
}

void VisitedLinkReader::AddVisitedLinks(
    const std::vector<VisitedLinkReader::Fingerprint>& fingerprints) {
  for (size_t i = 0; i < fingerprints.size(); ++i)
    WebView::UpdateVisitedLinkState(fingerprints[i]);
}

void VisitedLinkReader::ResetVisitedLinks(bool invalidate_hashes) {
  WebView::ResetVisitedLinkState(invalidate_hashes);
}

void VisitedLinkReader::UpdateOriginSalts(
    const base::flat_map<url::Origin, uint64_t>& origin_salts) {
  for (const auto& [origin, salt] : origin_salts) {
    base::UmaHistogramBoolean(
        "Blink.History.VisitedLinks.IsSaltFromNavigationThrottle", false);
    AddOrUpdateSalt(origin, salt);
  }
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
