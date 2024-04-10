// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/cluster_manager.h"

namespace commerce {

ClusterManager::ClusterManager() = default;

ClusterManager::~ClusterManager() = default;

void ClusterManager::WebWrapperDestroyed(const GURL& url) {}

void ClusterManager::DidNavigatePrimaryMainFrame(const GURL& url) {}

void ClusterManager::DidNavigateAway(const GURL& new_url,
                                     const GURL& from_url) {}

}  // namespace commerce
