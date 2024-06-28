// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITEDLINK_RENDERER_VISITEDLINK_READER_H_
#define COMPONENTS_VISITEDLINK_RENDERER_VISITEDLINK_READER_H_

#include "base/compiler_specific.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "components/visitedlink/common/visitedlink.mojom.h"
#include "components/visitedlink/common/visitedlink_common.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace visitedlink {

// Reads the link coloring database provided by the writer. There can be any
// number of readers reading the same database.
class VisitedLinkReader : public VisitedLinkCommon,
                          public mojom::VisitedLinkNotificationSink {
 public:
  VisitedLinkReader();

  VisitedLinkReader(const VisitedLinkReader&) = delete;
  VisitedLinkReader& operator=(const VisitedLinkReader&) = delete;

  ~VisitedLinkReader() override;

  base::RepeatingCallback<
      void(mojo::PendingReceiver<mojom::VisitedLinkNotificationSink>)>
  GetBindCallback();

  // Mutator for `salts_`. Called by Documents which received
  // per-origin salts from their navigation commit params. This function may
  // only be called on the UI (main) thread.
  void AddOrUpdateSalt(const url::Origin& origin, uint64_t salt) {
    salts_[origin] = salt;
  }

  // Determines the per-origin salt required to hash this triple-partition key
  // and returns the resulting fingerprint. If a valid fingerprint cannot be
  // computed, returns 0 ("the null fingerprint").
  uint64_t ComputePartitionedFingerprint(
      std::string_view canonical_link_url,
      const net::SchemefulSite& top_level_site,
      const url::Origin& frame_origin);

  // mojom::VisitedLinkNotificationSink overrides.
  void UpdateVisitedLinks(
      base::ReadOnlySharedMemoryRegion table_region) override;
  void AddVisitedLinks(
      const std::vector<VisitedLinkReader::Fingerprint>& fingerprints) override;
  void ResetVisitedLinks(bool invalidate_hashes) override;
  void UpdateOriginSalts(
      const base::flat_map<url::Origin, uint64_t>& origin_salts) override;

 private:
  void FreeTable();

  // These helper functions for UpdateVisitedLinks() allow us to use the correct
  // header (SharedHeader or PartitionedSharedHeader) when reading the new
  // shared memory region into our copy of the hashtable.
  void UpdateUnpartitionedVisitedLinks(
      base::ReadOnlySharedMemoryRegion table_region);
  void UpdatePartitionedVisitedLinks(
      base::ReadOnlySharedMemoryRegion table_region);

  void Bind(mojo::PendingReceiver<mojom::VisitedLinkNotificationSink> receiver);

  // Contains the per-origin salts required to hash every :visited link relevant
  // to this RenderProcess. Queries to the hashtable stored within
  // `table_mapping_` MUST provide the salt that corresponds to that link's
  // origin, otherwise :visited status cannot be determined.
  std::map<url::Origin, uint64_t> salts_;

  base::ReadOnlySharedMemoryMapping table_mapping_;

  mojo::Receiver<mojom::VisitedLinkNotificationSink> receiver_{this};

  base::WeakPtrFactory<VisitedLinkReader> weak_factory_{this};
};

}  // namespace visitedlink

#endif  // COMPONENTS_VISITEDLINK_RENDERER_VISITEDLINK_READER_H_
