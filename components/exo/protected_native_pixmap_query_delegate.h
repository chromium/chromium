// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_PROTECTED_NATIVE_PIXMAP_QUERY_DELEGATE_H_
#define COMPONENTS_EXO_PROTECTED_NATIVE_PIXMAP_QUERY_DELEGATE_H_

#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"

namespace exo {
// Interface for querying if a GMB handle is associated with a protected native
// pixmap. This is needed for platforms with HW protected video so we set the
// properties on the quad correctly.
class ProtectedNativePixmapQueryDelegate {
 public:
  virtual ~ProtectedNativePixmapQueryDelegate() = default;

  using IsProtectedNativePixmapHandleCallback = base::OnceCallback<void(bool)>;
  // Queries the GPU process for whether or not the passed in handle is
  // associated with protected native pixmap. Invokes the callback
  // asynchronously with the result.
  virtual void IsProtectedNativePixmapHandle(
      base::ScopedFD handle,
      IsProtectedNativePixmapHandleCallback callback) = 0;
};
}  // namespace exo
#endif  // COMPONENTS_EXO_PROTECTED_NATIVE_PIXMAP_QUERY_DELEGATE_H_