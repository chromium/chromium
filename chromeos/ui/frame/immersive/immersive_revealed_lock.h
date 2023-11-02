// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_IMMERSIVE_IMMERSIVE_REVEALED_LOCK_H_
#define CHROMEOS_UI_FRAME_IMMERSIVE_IMMERSIVE_REVEALED_LOCK_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"

namespace chromeos {

// Class which keeps the top-of-window views revealed for the duration of its
// lifetime. If acquiring the lock causes a reveal, the top-of-window views
// will animate according to the |animate_reveal| parameter passed in the
// constructor. See ImmersiveFullscreenController::GetRevealedLock() for more
// details.
class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) ImmersiveRevealedLock {
 public:
  class COMPONENT_EXPORT(CHROMEOS_UI_FRAME) Delegate {
   public:
    enum AnimateReveal { ANIMATE_REVEAL_YES, ANIMATE_REVEAL_NO };

    virtual void LockRevealedState(AnimateReveal animate_reveal) = 0;
    virtual void UnlockRevealedState() = 0;

   protected:
    virtual ~Delegate() {}
  };

  ImmersiveRevealedLock(const base::WeakPtr<Delegate>& delegate,
                        Delegate::AnimateReveal animate_reveal);

  ImmersiveRevealedLock(const ImmersiveRevealedLock&) = delete;
  ImmersiveRevealedLock& operator=(const ImmersiveRevealedLock&) = delete;

  ~ImmersiveRevealedLock();

 private:
  base::WeakPtr<Delegate> delegate_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_IMMERSIVE_IMMERSIVE_REVEALED_LOCK_H_
