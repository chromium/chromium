// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEDERATED_LEARNING_FLOC_ID_H_
#define COMPONENTS_FEDERATED_LEARNING_FLOC_ID_H_

#include "base/optional.h"
#include "base/version.h"

#include <stdint.h>

#include <string>
#include <unordered_set>

namespace federated_learning {

// ID used to represent a cohort of people with similar browsing habits. For
// more context, see the explainer at
// https://github.com/jkarlin/floc/blob/master/README.md
class FlocId {
 public:
  static uint64_t SimHashHistory(
      const std::unordered_set<std::string>& domains);

  FlocId();
  explicit FlocId(uint64_t id, uint32_t sorting_lsh_version);
  FlocId(const FlocId& id);

  ~FlocId();
  FlocId& operator=(const FlocId& id);
  FlocId& operator=(FlocId&& id);

  bool operator==(const FlocId& other) const;
  bool operator!=(const FlocId& other) const;

  bool IsValid() const;
  uint64_t ToUint64() const;

  // The id, followed by the chrome floc version, followed by the async floc
  // component versions (i.e. model and sorting-lsh). This is the format to be
  // exposed to the JS API. Precondition: |id_| must be valid.
  std::string ToString() const;

 private:
  base::Optional<uint64_t> id_;

  // The main version (i.e. 1st int) of the sorting lsh component version.
  uint32_t sorting_lsh_version_ = 0;
};

}  // namespace federated_learning

#endif  // COMPONENTS_FEDERATED_LEARNING_FLOC_ID_H_
