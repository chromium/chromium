// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SECURITY_DELEGATE_H_
#define COMPONENTS_EXO_SECURITY_DELEGATE_H_

#include <memory>
#include <string>

namespace aura {
class Window;
}

namespace exo {

// Each wayland server managed by exo, including the default server, will have a
// single delegate associated with it to control security-sensitive features of
// the server, e.g.:
//  - Availability of privileged APIs used by trusted clients only.
//  - Handling of certain mechanisms differently for different products (arc,
//    crostini, etc)
// This allows exo to make strong guarantees about the relationship between the
// wl clients and the SecurityDelegate the server owns.
//
// See go/secure-exo-ids and go/securer-exo-ids for more details.
class SecurityDelegate {
 public:
  // See |CanSetBounds()|.
  enum SetBoundsPolicy {
    // By default, clients may not set window bounds. Requests are ignored.
    IGNORE,

    // Clients may set bounds, but Exo may DCHECK on requests for windows with
    // server-side decoration.
    DCHECK_IF_DECORATED,

    // Clients may set bounds for any window. Exo will expand the requested
    // bounds to account for server-side decorations, if any.
    ADJUST_IF_DECORATED,
  };

  // Get a SecurityDelegate instance with all of the defaults.
  static std::unique_ptr<SecurityDelegate> GetDefaultSecurityDelegate();

  virtual ~SecurityDelegate();

  // "Self-activation" is a security sensitive windowing operation that is a
  // common paradigm in X11. The need to self-activate is controlled
  // per-subsystem, i.e. a product like ARC++ knows that its windows should be
  // able to self activate, whereas Crostini knows they usually shouldn't.
  virtual bool CanSelfActivate(aura::Window* window) const;

  // Called when a client made pointer lock request, defined in
  // pointer-constraints-unstable-v1.xml extension protocol.  True if the client
  // can lock the location of the pointer and disable movement, or return false
  // to reject the pointer lock request.
  virtual bool CanLockPointer(aura::Window* window) const;

  // Whether clients may set their own windows' bounds is a security-relevant
  // policy decision.
  //
  // If server-side decoration is used, clients normally should not set their
  // own window bounds, as they may not be able to compute them correctly
  // (accounting for the size of the window decorations).
  virtual SetBoundsPolicy CanSetBounds(aura::Window* window) const;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SECURITY_DELEGATE_H_
