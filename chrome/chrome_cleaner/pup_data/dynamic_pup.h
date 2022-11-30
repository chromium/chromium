// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PUP_DATA_DYNAMIC_PUP_H_
#define CHROME_CHROME_CLEANER_PUP_DATA_DYNAMIC_PUP_H_

#include <string>

#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace chrome_cleaner {

// A subclass of PUP that manages memory for its signature. Standard PUP
// structures point their |signature_| to a static footprint. The signature for
// this PUP is dynamically created, so it also stores the full signature in
// |stored_signature_|, with |signature_| pointing to it. To avoid invalidating
// |signature_|, DynamicPUP cannot be copied or moved.
class DynamicPUP : public PUPData::PUP {
 public:
  DynamicPUP(const std::string& name, UwSId id, PUPData::Flags flags);

  DynamicPUP(const DynamicPUP& other) = delete;
  DynamicPUP(DynamicPUP&&) = delete;
  DynamicPUP& operator=(const DynamicPUP&&) = delete;
  DynamicPUP& operator=(DynamicPUP&&) = delete;

 private:
  void UpdateStoredName(const std::string& name);

  // The |name| member of |stored_signature_| will point to this string.
  std::string stored_name_;

  PUPData::UwSSignature stored_signature_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PUP_DATA_DYNAMIC_PUP_H_
