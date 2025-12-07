// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKLIST_OPT_OUT_BLOCKLIST_OPT_OUT_STORE_H_
#define COMPONENTS_BLOCKLIST_OPT_OUT_BLOCKLIST_OPT_OUT_STORE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_data.h"

namespace blocklist {

typedef base::OnceCallback<void(std::unique_ptr<BlocklistData>)>
    LoadBlockListCallback;

// OptOutStore keeps opt out information for the blocklist.
// Ability to create multiple instances of the store as well as behavior of
// asynchronous operations when the object is being destroyed, before such
// operation finishes will depend on implementation. It is possible to issue
// multiple asynchronous operations in parallel and maintain ordering.
class OptOutStore {
 public:
  virtual ~OptOutStore() = default;

  // Adds a new navigation to the store. |opt_out| is whether the user opted out
  // of the action.
  virtual void AddEntry(bool opt_out,
                        const std::string& host_name,
                        int type,
                        base::Time now) = 0;

  // Asynchronously loads a map of host names to OptOutBlocklistItem for that
  // host from the store. And runs |callback| once loading is finished.
  virtual void LoadBlockList(std::unique_ptr<BlocklistData> blocklist_data,
                             LoadBlockListCallback callback) = 0;

  // Deletes all history in the store between |begin_time| and |end_time|.
  virtual void ClearBlockList(base::Time begin_time, base::Time end_time) = 0;
};

}  // namespace blocklist

#endif  // COMPONENTS_BLOCKLIST_OPT_OUT_BLOCKLIST_OPT_OUT_STORE_H_
