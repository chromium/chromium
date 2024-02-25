// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYNC_INVALIDATION_H_
#define COMPONENTS_SYNC_BASE_SYNC_INVALIDATION_H_

#include <stdint.h>

#include <string>

namespace syncer {

// An interface that wraps sync's interactions with the component that provides
// it with invalidations.
class SyncInvalidation {
 public:
  // Orders invalidations based on version number and IsUnknownVersion().
  static bool LessThanByVersion(const SyncInvalidation& a,
                                const SyncInvalidation& b);

  SyncInvalidation();
  virtual ~SyncInvalidation();

  // Returns true if this is an 'unknown version' invalidation.
  // Such invalidations have no valid payload or version number.
  virtual bool IsUnknownVersion() const = 0;

  // Returns the payload of this item.
  virtual const std::string& GetPayload() const = 0;

  // Returns the version of this item.
  // DCHECKs if this is an unknown version invalidation.
  //
  // It is preferable to use the LessThan() function, which handles unknown
  // versions properly, rather than this function.
  virtual int64_t GetVersion() const = 0;

  // This function will be called when the invalidation has been handled
  // successfully.
  virtual void Acknowledge() = 0;

  // This function should be called if a lack of buffer space required that we
  // drop this invalidation.
  //
  // To indicate recovery from a drop event, the receiver of this invalidation
  // will call Acknowledge() on the most recently dropped invalidation.
  virtual void Drop() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SYNC_INVALIDATION_H_
