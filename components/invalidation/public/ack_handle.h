// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_ACK_HANDLE_H_
#define COMPONENTS_INVALIDATION_PUBLIC_ACK_HANDLE_H_

#include <string>

#include "base/time/time.h"
#include "components/invalidation/public/invalidation_export.h"

namespace invalidation {

// Opaque class that represents a local ack handle. We don't reuse the
// invalidation ack handles to avoid unnecessary dependencies.
class INVALIDATION_EXPORT AckHandle {
 public:
  AckHandle();
  AckHandle(const AckHandle& other);
  AckHandle& operator=(const AckHandle& other);
  ~AckHandle();

  bool Equals(const AckHandle& other) const;

 private:
  std::string state_;
  base::Time timestamp_;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_ACK_HANDLE_H_
