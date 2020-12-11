// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_RENOVATIONS_PAGE_RENOVATOR_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_RENOVATIONS_PAGE_RENOVATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/offline_pages/core/renovations/page_renovation_loader.h"
#include "components/offline_pages/core/renovations/script_injector.h"
#include "components/offline_pages/core/snapshot_controller.h"

namespace offline_pages {

// This class runs renovations in a page being offlined. Upon
// construction, it determines which renovations to run. The user
// should then call RunRenovations when the page is sufficiently
// loaded. When complete (if ever) the passed CompletionCallback will
// be called.
class PageRenovator {
 public:
  using CompletionCallback = base::OnceClosure;

  // |renovation_loader| should be owned by the user and is expected to
  // outlive this PageRenovator instance.
  PageRenovator(PageRenovationLoader* renovation_loader,
                std::unique_ptr<ScriptInjector> script_injector,
                const GURL& request_url);
  ~PageRenovator();

  // Run renovation scripts and call callback when they complete.
  void RunRenovations(CompletionCallback callback);

 private:
  // This method is used in the constructor to determine which
  // renovations to run and populate |script_| with the renovations.
  void PrepareScript(const GURL& url);

  PageRenovationLoader* renovation_loader_;
  std::unique_ptr<ScriptInjector> script_injector_;
  base::string16 script_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_RENOVATIONS_PAGE_RENOVATOR_H_
