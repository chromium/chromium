// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_PROTO_MEMORY_ESTIMATIONS_H_
#define COMPONENTS_SYNC_PROTOCOL_PROTO_MEMORY_ESTIMATIONS_H_

#include <stddef.h>

namespace sync_pb {

// Estimates memory usage for a proto.
// Needs to be in sync_pb namespace for ADL to find it (when for example
// EstimateMemoryUsage() is called on a list of protos).
// Note: if you get linking errors you need to add explicit instantiation at
// the end of the implementation file.
template <class P>
size_t EstimateMemoryUsage(const P& proto);

}  // namespace sync_pb

#endif  // COMPONENTS_SYNC_PROTOCOL_PROTO_MEMORY_ESTIMATIONS_H_
