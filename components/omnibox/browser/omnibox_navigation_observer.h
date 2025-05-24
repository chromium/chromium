// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_NAVIGATION_OBSERVER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_NAVIGATION_OBSERVER_H_

// Interface that allows an embedder to monitor omnibox navigations in order to
// trigger behaviors that depend on successful navigations.
//
// The memory management of this object is a bit tricky. On opening a match,
// the OmniboxEditModel will ask the OmniboxClient to create us if necessary.
// Once we are created, OmniboxEditModel will be responsible for us until we
// reach the state where we have seen a pending load (it will delete us if this
// doesn't happen by the time that processing the match has finished)). Once we
// have seen a pending load, we're responsible for deleting ourselves at
// whatever time we deem is appropriate.

class OmniboxNavigationObserver {
 public:
  virtual ~OmniboxNavigationObserver() = default;

  // Returns true iff this observer has seen a pending load since its
  // creation.
  virtual bool HasSeenPendingLoad() const = 0;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_NAVIGATION_OBSERVER_H_
