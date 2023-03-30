// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_

#include "components/offline_pages/core/prefetch/prefetch_service.h"

namespace offline_pages {

// TODO(crbug.com/1424920): This is in the process of being deleted. Remove this
// code after it's been live for one milestone.
class PrefetchServiceImpl : public PrefetchService {
 public:
  PrefetchServiceImpl();

  PrefetchServiceImpl(const PrefetchServiceImpl&) = delete;
  PrefetchServiceImpl& operator=(const PrefetchServiceImpl&) = delete;

  ~PrefetchServiceImpl() override;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_SERVICE_IMPL_H_
