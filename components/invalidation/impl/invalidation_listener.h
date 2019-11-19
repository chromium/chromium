// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LISTENER_H_
#define COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LISTENER_H_

#include <string>

namespace syncer {

// Handlers registration and message recieving events.
class InvalidationListener {
 public:
  virtual ~InvalidationListener() {}

  /* Indicates that an object has been updated to a particular version.
   *
   * It is guaranteed that this callback will be invoked at least once for
   * every invalidation that it guaranteed to deliver. It does not guarantee
   * exactly-once delivery or in-order delivery (with respect to the version
   * number).
   *
   *     payload - additional info specific to the invalidations
   *     version - version of the invalidation
   *     private_topic - the internal (to FCM) representation for the public
   *     topic.
   *     public_topic - the topic, which was invalidated, e.g. in case of Chrome
   *     Sync it'll be BOOKMARK or PASSWORD
   */
  virtual void Invalidate(const std::string& payload,
                          const std::string& private_topic,
                          const std::string& public_topic,
                          const std::string& version) = 0;

  /* Informs the listener about new token being available.
   *     token - instance id token, received from the network.
   */
  virtual void InformTokenReceived(const std::string& token) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_INVALIDATION_LISTENER_H_
