// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_SINGULAR_UKM_ENTRY_H_
#define COMPONENTS_UKM_SINGULAR_UKM_ENTRY_H_

#include <memory>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace ukm {

template <typename>
class SingularUkmEntry;

// Creates the SingularUkmInterface receiver.
void CreateSingularUkmInterface(
    mojo::PendingReceiver<mojom::SingularUkmInterface> receiver);

// Singular UKM Entries are UKM Entries that can be 'recorded' multiple times
// but only the last one will be recorded by the UKM Service. While not
// currently supported, it is designed to support entries from multiple
// processes and still record the UkmEntry in the event that the remote process
// is exited. If the browser process crashes then all entries that haven't been
// saved to a log are lost.
//
// Notice: This currently only works within the browser process. Multi-process
//         is not implemented and changes need to be added to the startup
//         process of the desired process.
//
// Create a new UKM entry as so:
//
// std::unique_ptr<SingularUkmEntry<AdFrameLoad>> entry =
//    SingularUkmEntry<AdFrameLoad>::Create(source_id);
//
// {
//   SingularUkmEntry<AdFrameLoad>::EntryBuilder builder = entry->Builder();
//   builder->SetCpuTime_PeakWindowedPercent(cpu_time_pwp1);
//   builder->SetCpuTime_Total(total_cpu_time1);
// }
// {
//   SingularUkmEntry<AdFrameLoad>::EntryBuilder builder = entry->Builder();
//   builder->SetCpuTime_PeakWindowedPercent(cpu_time_pwp2);
//   builder->SetCpuTime_Total(total_cpu_time2);
// }
//
// In the example above, only cpu_time_pwp2 and total_cpu_time2 will be recorded
// by the UKM service when `entry` is destroyed. Destructor of `builder` will
// update the `entry`.
//
// Note:
// - Use arrow operator on the builder to access the underlying UkmEntry. This
//   will give access to UkmEntry's methods.
// - When a EntryBuilder is destroyed or goes out of scope, the corresponding
//   UkmEntry is committed as the latest to be recorded.
// - A SingularUkmEntry and the EntryBuilders created from it must be used on
//   the same sequence it was created.
// - The UkmEntry will not be recorded until the SingularUkmEntry is destroyed.
// - The SingularUkmEntry must outlive all EntryBuilder's created from it.
// - All desired metrics must be set by the EntryBuilder to be recorded.
//
// Wrapper class to associate an UkmEntry type with an interface and source id.
// Template UkmEntry is expected to have a base class of UkmEntryBuilderBase.
// See tools/metrics/ukm/ukm.xml for entry definitions.
template <typename UkmEntry>
class SingularUkmEntry {
 public:
  // Creates a new SingularUkmEntry.
  static std::unique_ptr<SingularUkmEntry<UkmEntry>> Create(
      SourceId source_id) {
    mojo::PendingRemote<mojom::SingularUkmInterface> remote;
    CreateSingularUkmInterface(remote.InitWithNewPipeAndPassReceiver());

    return base::WrapUnique(
        new SingularUkmEntry<UkmEntry>(source_id, std::move(remote)));
  }

  SingularUkmEntry(const SingularUkmEntry<UkmEntry>&) = delete;
  SingularUkmEntry& operator=(const SingularUkmEntry<UkmEntry>&) = delete;

  ~SingularUkmEntry() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  // Provides an interface to build the UkmEntry. The built UkmEntry is
  // submitted to the interface on destruction.
  class EntryBuilder {
   public:
    EntryBuilder(const EntryBuilder&) = delete;
    EntryBuilder& operator=(const EntryBuilder&) = delete;

    ~EntryBuilder() {
      DCHECK_CALLED_ON_VALID_SEQUENCE(singular_ukm_entry_->sequence_checker_);
      singular_ukm_entry_->interface_->Submit(entry_.TakeEntry());
    }

    // Provides a simple interface to interact with the underlying UkmEntry.
    UkmEntry* operator->() { return &entry_; }

   private:
    friend class SingularUkmEntry<UkmEntry>;

    EntryBuilder(SourceId source_id,
                 SingularUkmEntry<UkmEntry>& singular_ukm_entry)
        : entry_(source_id), singular_ukm_entry_(&singular_ukm_entry) {}

    // The current entry being built.
    UkmEntry entry_;

    // The SingularUkmEntry that created |this|. It provides the Mojo interface
    // to submit completed UKMEntryies.
    raw_ptr<SingularUkmEntry<UkmEntry>> singular_ukm_entry_;
  };

  // Creates an EntryBuilder. EntryBuilders are used to build an UkmEntry and
  // submit it to the receiver.
  EntryBuilder Builder() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return EntryBuilder(source_id_, *this);
  }

 private:
  SingularUkmEntry(SourceId source_id,
                   mojo::PendingRemote<mojom::SingularUkmInterface> interface)
      : source_id_(source_id), interface_(std::move(interface)) {
    CHECK(interface_);
  }

  // The SourceId to be used by new entries.
  SourceId source_id_;

  mojo::Remote<mojom::SingularUkmInterface> interface_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_SINGULAR_UKM_ENTRY_H_
