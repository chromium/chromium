// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "url/gurl.h"

namespace autofill_assistant {

// Utility that implements a heuristic for autofill-assistant URLs.
//
// This class inherits from RefCountedThreadSafe to allow safe evaluation on
// worker threads.
class StarterHeuristic : public base::RefCountedThreadSafe<StarterHeuristic> {
 public:
  StarterHeuristic();
  StarterHeuristic(const StarterHeuristic&) = delete;
  StarterHeuristic& operator=(const StarterHeuristic&) = delete;

  // Runs the heuristic against |url|. Returns true if a trigger script might be
  // available for |url|. This method runs on a worker thread and notifies the
  // caller via |callback| when done.
  void RunHeuristicAsync(const GURL& url,
                         base::OnceCallback<void(bool result)> callback) const;

 private:
  friend class base::RefCountedThreadSafe<StarterHeuristic>;
  friend class StarterHeuristicTest;
  ~StarterHeuristic();

  // Runs the heuristic against |url|. Returns true if a trigger script might be
  // available for |url|.
  bool IsHeuristicMatch(const GURL& url) const;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_H_
