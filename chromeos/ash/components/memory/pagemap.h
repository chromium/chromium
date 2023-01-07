// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEMORY_PAGEMAP_H_
#define CHROMEOS_ASH_COMPONENTS_MEMORY_PAGEMAP_H_

#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"

namespace ash {
namespace memory {

// Pagemap fetches pagemap entries from procfs for a process.
class COMPONENT_EXPORT(ASH_MEMORY) Pagemap {
 public:
  // For more information on the Pagemap layout see the kernel documentation at
  // https://www.kernel.org/doc/Documentation/vm/pagemap.txt
  struct PagemapEntry {
    uint8_t swap_type : 5;
    uint64_t swap_offset : 50;
    bool pte_soft_dirty : 1;
    bool page_exclusively_mapped : 1;
    uint8_t : 4;  // these bits are unused
    bool page_file_or_shared_anon : 1;
    bool page_swapped : 1;
    bool page_present : 1;
  } __attribute__((packed));

  static_assert(sizeof(PagemapEntry) == sizeof(uint64_t),
                "PagemapEntry is expected to be 8 bytes");

  explicit Pagemap(pid_t pid);

  Pagemap(const Pagemap&) = delete;
  Pagemap& operator=(const Pagemap&) = delete;

  ~Pagemap();

  bool IsValid() const;

  // GetEntries will populate |entries| for the memory region specified.
  // It is required that |address| be page aligned and |length| must always be a
  // page length multiple. Additionally, |entries| is expected to be sized to
  // the number of pages in the range specified. If it's not properly sized it
  // will be resized to the appropriate length for the caller.
  bool GetEntries(uint64_t address,
                  uint64_t length,
                  std::vector<PagemapEntry>* entries) const;

  // GetNumberOfPagesPresent is a helper which makes it easy to quickly
  // determine the number of pages in core for the region specified by |address|
  // and |length|. On success the function will return true and pages_present
  // will be set accordingly.
  bool GetNumberOfPagesPresent(uint64_t address,
                               uint64_t length,
                               uint64_t* pages_present) const;

  // IsFullyPresent is a small helper around GetNumberOfPagesPresent which
  // returns true if every page in the range is presnt.
  bool IsFullyPresent(uint64_t address, uint64_t length) const;

 private:
  friend class PagemapTest;

  base::ScopedFD fd_;
};

}  // namespace memory
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEMORY_PAGEMAP_H_
