// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_POINTER_CONSTRAINT_DELEGATE_H_
#define COMPONENTS_EXO_POINTER_CONSTRAINT_DELEGATE_H_

namespace exo {

class Surface;

class PointerConstraintDelegate {
 public:
  virtual ~PointerConstraintDelegate() = default;

  // Called when this lock is broken for any reason. Possibly:
  //  - A user action broke the lock.
  //  - The lock was granted to a different client.
  //  - The pointer was destroyed while the lock was active.
  //
  // No matter the case, this delegate no longer holds the lock and therefore
  // should not call UnconstrainPointer().
  virtual void OnConstraintBroken() = 0;

  // Callback to access the surface which this delegate wants to lock the
  // curstor for.
  virtual Surface* GetConstrainedSurface() = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_POINTER_CONSTRAINT_DELEGATE_H_
