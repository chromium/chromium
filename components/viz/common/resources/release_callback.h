// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RELEASE_CALLBACK_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RELEASE_CALLBACK_H_

#include "base/functional/callback.h"

namespace gpu {
struct SyncToken;
}  // namespace gpu

namespace viz {

// The callback type used to return ownership of a resource.
// The |sync_token| is a synchronization point that should be waited for before
// using the resource. Failing to do so may cause corruption as the resource may
// still be being acted on by the system releasing ownership until that point.
// When |is_lost| is true, the resource is declared as unable to be returned,
// and the receiver of this callback may not reliably reuse the resource and
// guarantee any correctness. The receiver of this callback may only delete the
// resource when |is_lost| is set.
using ReleaseCallback =
    base::OnceCallback<void(const gpu::SyncToken& sync_token, bool is_lost)>;

// The callback type used to denote the eviction of a Surface for which a
// resource is referenced. When invoked the receiver of this callback should
// remove the resource, and no longer attempt to reuse it. However it is not
// safe to delete the resource at this time. A subsequent notification of
// `ReleaseCallback` will be sent once all references have been removed, at
// which point it will be safe to delete.
using ResourceEvictedCallback = base::OnceClosure;

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RELEASE_CALLBACK_H_
