// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_

#include "base/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}

namespace performance_manager {

class Graph;
class GraphOwned;
class PageNode;

// The performance manager is a rendezvous point for communicating with the
// performance manager graph on its dedicated sequence.
class PerformanceManager {
 public:
  // Returns true if the performance manager is initialized. Valid to call from
  // the main thread only.
  static bool IsAvailable();

  // Posts a callback that will run on the PM sequence, and be provided a
  // pointer to the Graph. Valid to call from any sequence, but |graph_callback|
  // won't run if "IsAvailable" returns false.
  using GraphCallback = base::OnceCallback<void(Graph*)>;
  static void CallOnGraph(const base::Location& from_here,
                          GraphCallback graph_callback);

  // Passes a GraphOwned object into the Graph on the PM sequence. Should only
  // be called from the main thread and only if "IsAvailable" returns true.
  static void PassToGraph(const base::Location& from_here,
                          std::unique_ptr<GraphOwned> graph_owned);

  // Returns a WeakPtr to the PageNode associated with a given WebContents,
  // or a null WeakPtr if there's no PageNode for this WebContents.
  // Valid to call from the main thread only, the returned WeakPtr should only
  // be dereferenced on the PM sequence (e.g. it can be used in a
  // CallOnGraph callback).
  static base::WeakPtr<PageNode> GetPageNodeForWebContents(
      content::WebContents* wc);

 protected:
  PerformanceManager();
  virtual ~PerformanceManager();

 private:
  DISALLOW_COPY_AND_ASSIGN(PerformanceManager);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_
