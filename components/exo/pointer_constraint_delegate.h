// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_POINTER_CONSTRAINT_DELEGATE_H_
#define COMPONENTS_EXO_POINTER_CONSTRAINT_DELEGATE_H_

namespace exo {

class Pointer;
class Surface;

class PointerConstraintDelegate {
 public:
  virtual ~PointerConstraintDelegate() = default;

  // Called when the lock is activated by the compositor.
  //
  // For non-persistent ("one-shot") locks, this may be called 0 or 1 times.
  // For persistent locks, this may be called again after OnConstraintBroken().
  virtual void OnConstraintActivated() = 0;

  // Called when the lock is not activated by the compositor because a
  // pointer constraint was already requested on this surface.
  virtual void OnAlreadyConstrained() = 0;

  // Called when this lock is broken for any reason. Possibly:
  //  - A user action broke the lock.
  //  - The lock was granted to a different client.
  //  - The pointer was destroyed while the lock was active.
  virtual void OnConstraintBroken() = 0;

  // Whether the lock is "persistent", meaning it can be reactivated by the
  // compositor after being broken.
  virtual bool IsPersistent() = 0;

  // Returns the surface which this delegate wants to lock the cursor for.
  // The delegate does not guarantee that this pointer is valid, except
  // when calling Pointer::ConstrainPointer(); the caller is responsible for
  // tracking the Surface's lifetime.
  virtual Surface* GetConstrainedSurface() = 0;

  // Notifies the delegate that it's defunct and must not call
  // Pointer::OnPointerConstraintDelegateDestroying().
  virtual void OnDefunct() = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_POINTER_CONSTRAINT_DELEGATE_H_
