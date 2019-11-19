// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLACKLIST_OPT_OUT_BLACKLIST_OPT_OUT_BLACKLIST_DELEGATE_H_
#define COMPONENTS_BLACKLIST_OPT_OUT_BLACKLIST_OPT_OUT_BLACKLIST_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"

namespace blacklist {

// An interface for a delegate to the opt out blacklist. This interface is for
// responding to events occurring in the opt out blacklist (e.g. New blacklisted
// host and user is blacklisted).
class OptOutBlacklistDelegate {
 public:
  OptOutBlacklistDelegate() {}
  virtual ~OptOutBlacklistDelegate() {}

  // Notifies |this| that |host| has been blacklisted at |time|. This method is
  // guaranteed to be called when a previously whitelisted host is now
  // blacklisted.
  virtual void OnNewBlacklistedHost(const std::string& host, base::Time time) {}

  // Notifies |this| that the user blacklisted has changed, and it is
  // guaranteed to be called when the user blacklisted status is changed.
  virtual void OnUserBlacklistedStatusChange(bool blacklisted) {}

  // Notifies |this| that the blacklist is cleared at |time|.
  virtual void OnBlacklistCleared(base::Time time) {}
};

}  // namespace blacklist

#endif  // COMPONENTS_BLACKLIST_OPT_OUT_BLACKLIST_OPT_OUT_BLACKLIST_DELEGATE_H_
