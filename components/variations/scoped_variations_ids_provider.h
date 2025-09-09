// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SCOPED_VARIATIONS_IDS_PROVIDER_H_
#define COMPONENTS_VARIATIONS_SCOPED_VARIATIONS_IDS_PROVIDER_H_

#include "components/variations/variations_ids_provider.h"

namespace variations::test {

class ScopedVariationsIdsProvider {
 public:
  explicit ScopedVariationsIdsProvider(VariationsIdsProvider::Mode mode);
  ScopedVariationsIdsProvider(const ScopedVariationsIdsProvider&) = delete;
  ScopedVariationsIdsProvider& operator=(const ScopedVariationsIdsProvider&) =
      delete;
  ~ScopedVariationsIdsProvider();
};

}  // namespace variations::test

namespace variations {
// TODO: crbug.com/442849530 - There are some uses in other repos that can't be
// changed atomically. Thus, a compatibility alias is needed for now. This can
// go away once all uses in other repos are migrated to the new namespace.
using ScopedVariationsIdsProvider =
    ::variations::test::ScopedVariationsIdsProvider;
}  //  namespace variations

#endif  // COMPONENTS_VARIATIONS_SCOPED_VARIATIONS_IDS_PROVIDER_H_
