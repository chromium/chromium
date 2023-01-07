// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKLIST_OPT_OUT_BLOCKLIST_OPT_OUT_BLOCKLIST_DELEGATE_H_
#define COMPONENTS_BLOCKLIST_OPT_OUT_BLOCKLIST_OPT_OUT_BLOCKLIST_DELEGATE_H_

#include <string>

#include "base/time/time.h"

namespace blocklist {

// An interface for a delegate to the opt out blocklist. This interface is for
// responding to events occurring in the opt out blocklist (e.g. New blocklisted
// host and user is blocklisted).
class OptOutBlocklistDelegate {
 public:
  OptOutBlocklistDelegate() = default;
  virtual ~OptOutBlocklistDelegate() = default;

  // Notifies |this| that |host| has been blocklisted at |time|. This method is
  // guaranteed to be called when a previously allowlisted host is now
  // blocklisted.
  virtual void OnNewBlocklistedHost(const std::string& host, base::Time time) {}

  // Notifies |this| that the user blocklisted has changed, and it is
  // guaranteed to be called when the user blocklisted status is changed.
  virtual void OnUserBlocklistedStatusChange(bool blocklisted) {}

  // Notifies |this| the blocklist loaded state changed to |is_loaded|.
  virtual void OnLoadingStateChanged(bool is_loaded) {}

  // Notifies |this| that the blocklist is cleared at |time|.
  virtual void OnBlocklistCleared(base::Time time) {}
};

}  // namespace blocklist

#endif  // COMPONENTS_BLOCKLIST_OPT_OUT_BLOCKLIST_OPT_OUT_BLOCKLIST_DELEGATE_H_
