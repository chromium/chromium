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

  // mojom::VisitedLinkNotificationSink overrides.
  void UpdateVisitedLinks(
      base::ReadOnlySharedMemoryRegion table_region) override;
  void AddVisitedLinks(
      const std::vector<VisitedLinkReader::Fingerprint>& fingerprints) override;
  void ResetVisitedLinks(bool invalidate_hashes) override;

 private:
  void FreeTable();

  void Bind(mojo::PendingReceiver<mojom::VisitedLinkNotificationSink> receiver);

  base::ReadOnlySharedMemoryMapping table_mapping_;

  mojo::Receiver<mojom::VisitedLinkNotificationSink> receiver_{this};

  base::WeakPtrFactory<VisitedLinkReader> weak_factory_{this};
};

}  // namespace visitedlink

#endif  // COMPONENTS_VISITEDLINK_RENDERER_VISITEDLINK_READER_H_
