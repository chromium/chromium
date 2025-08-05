// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_OBSERVER_H_
#define COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_OBSERVER_H_

#include "components/search_provider_logos/logo_common.h"

namespace search_provider_logos {

// Receives updates when the search provider's logo is available.
class LogoObserver {
 public:
  virtual ~LogoObserver() = default;

  // Called when the cached logo is available and possibly when a freshly
  // downloaded logo is available. |logo| will be NULL if no logo is available.
  // |from_cache| indicates whether the logo was loaded from the cache.
  //
  // If the fresh logo is the same as the cached logo, this will not be called
  // again.
  virtual void OnLogoAvailable(const Logo* logo, bool from_cache) = 0;

  // Called when the LogoService will no longer send updates to this
  // LogoObserver. For example: after the cached logo is validated, after
  // OnFreshLogoAvailable() is called, or when the LogoService is destructed.
  // This is not called when an observer is removed using RemoveObserver().
  virtual void OnObserverRemoved() = 0;
};

}  // namespace search_provider_logos

#endif  // COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_OBSERVER_H_
