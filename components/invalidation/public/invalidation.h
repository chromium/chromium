// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_H_
#define COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_H_

#include <stdint.h>

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/invalidation/public/ack_handle.h"
#include "components/invalidation/public/invalidation_export.h"
#include "components/invalidation/public/invalidation_util.h"

namespace invalidation {

class AckHandler;

// Represents a local invalidation. This class supports "local" ack-tracking
// and simple serialization to pref values.
class INVALIDATION_EXPORT Invalidation {
 public:
  // Factory functions.
  static Invalidation Init(const Topic& topic,
                           int64_t version,
                           const std::string& payload);
  static Invalidation InitUnknownVersion(const Topic& topic);

  Invalidation(const Invalidation& other);
  Invalidation& operator=(const Invalidation& other);
  ~Invalidation();

  // Compares two invalidations.  The comparison ignores ack-tracking state.
  bool operator==(const Invalidation& other) const;

  Topic topic() const;
  bool is_unknown_version() const;

  // Safe to call only if is_unknown_version() returns false.
  int64_t version() const;

  // Safe to call only if is_unknown_version() returns false.
  const std::string& payload() const;

  const AckHandle& ack_handle() const;

  // Sets the AckHandler to be used to track this Invalidation.
  //
  // This should be set by the class that generates the invalidation.  Clients
  // of the Invalidations API should not need to call this.
  //
  // Note that some sources of invalidations do not support ack tracking, and do
  // not set the ack_handler.  This will be hidden from users of this class.
  void SetAckHandler(
      base::WeakPtr<AckHandler> handler,
      scoped_refptr<base::SequencedTaskRunner> handler_task_runner);

  // Returns whether or not this instance supports ack tracking.  This will
  // depend on whether or not the source of invaliadations supports
  // invalidations.
  //
  // Clients can safely ignore this flag.  They can assume that all
  // invalidations support ack tracking.  If they're wrong, then invalidations
  // will be less reliable, but their behavior will be no less correct.
  bool SupportsAcknowledgement() const;

  // Acknowledges the receipt of this invalidation.
  //
  // Clients should call this on a received invalidation when they have fully
  // processed the invalidation and persisted the results to disk.  Once this
  // function is called, the invalidations system is under no obligation to
  // re-deliver this invalidation in the event of a crash or restart.
  void Acknowledge() const;

  base::Value::Dict ToValue() const;
  std::string ToString() const;

 private:
  Invalidation(const Topic& topic,
               bool is_unknown_version,
               int64_t version,
               const std::string& payload,
               AckHandle ack_handle);

  // The Topic to which this invalidation belongs.
  Topic topic_;

  // This flag is set to true if this is an unknown version invalidation.
  bool is_unknown_version_;

  // The version number of this invalidation.  Should not be accessed if this is
  // an unkown version invalidation.
  int64_t version_;

  // The payaload associated with this invalidation.  Should not be accessed if
  // this is an unknown version invalidation.
  std::string payload_;

  // A locally generated unique ID used to manage local acknowledgements.
  AckHandle ack_handle_;

  // The acknowledgement tracking handler and its thread.
  base::WeakPtr<AckHandler> ack_handler_;
  scoped_refptr<base::SequencedTaskRunner> ack_handler_task_runner_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_INVALIDATION_H_
