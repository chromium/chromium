// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace commerce {

// Class for clustering product information.
class ClusterManager {
 public:
  ClusterManager();
  ~ClusterManager();
  ClusterManager(const ClusterManager&) = delete;
  ClusterManager& operator=(const ClusterManager&) = delete;

  // A notification that a WebWrapper with `url` has been destroyed. This
  // signals that the web page backing the provided WebWrapper is about to be
  // destroyed. Typically corresponds to a user closing a tab.
  void WebWrapperDestroyed(const GURL& url);
  // A notification that a web wrapper with `url` finished a navigation in the
  // primary main frame.
  void DidNavigatePrimaryMainFrame(const GURL& url);
  // A notification that the user navigated away from `from_url` to `new_url`.
  void DidNavigateAway(const GURL& new_url, const GURL& from_url);

 private:
  base::WeakPtrFactory<ClusterManager> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMPARE_CLUSTER_MANAGER_H_
