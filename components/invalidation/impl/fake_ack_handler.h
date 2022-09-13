// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FAKE_ACK_HANDLER_H_
#define COMPONENTS_INVALIDATION_IMPL_FAKE_ACK_HANDLER_H_

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "components/invalidation/public/ack_handler.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

class Invalidation;

// This AckHandler implementation colaborates with the FakeInvalidationService
// to enable unit tests to assert that invalidations are being acked properly.
class INVALIDATION_EXPORT FakeAckHandler
    : public AckHandler,
      public base::SupportsWeakPtr<FakeAckHandler> {
 public:
  FakeAckHandler();
  ~FakeAckHandler() override;

  // Sets up some internal state to track this invalidation, and modifies it so
  // that its Acknowledge() and Drop() methods will route back to us.
  void RegisterInvalidation(Invalidation* invalidation);

  // No one was listening for this invalidation, so no one will receive it or
  // ack it.  We keep track of it anyway to let tests make assertions about it.
  void RegisterUnsentInvalidation(Invalidation* invalidation);

  // Returns true if the specified invalidaition has been delivered, but has not
  // been acknowledged yet.
  bool IsUnacked(const Invalidation& invalidation) const;

  // Returns true if the specified invalidation has been delivered and
  // acknowledged.
  bool IsAcknowledged(const Invalidation& invalidation) const;

  // Returns true if the specified invalidation has been delivered and
  // dropped.
  bool IsDropped(const Invalidation& invalidation) const;

  // Returns true if the specified invalidation was never delivered.
  bool IsUnsent(const Invalidation& invalidation) const;

  // Retruns true if all invalidations have been acked and all drops recovered.
  bool AllInvalidationsAccountedFor() const;

  // Implementation of AckHandler.
  void Acknowledge(const Topic& topic, const AckHandle& handle) override;
  void Drop(const Topic& topic, const AckHandle& handle) override;

 private:
  typedef std::vector<Invalidation> InvalidationVector;

  InvalidationVector unsent_invalidations_;
  InvalidationVector unacked_invalidations_;
  InvalidationVector acked_invalidations_;
  InvalidationVector dropped_invalidations_;

  std::map<Topic, AckHandle> unrecovered_drop_events_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FAKE_ACK_HANDLER_H_
