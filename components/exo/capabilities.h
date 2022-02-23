// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_CAPABILITIES_H_
#define COMPONENTS_EXO_CAPABILITIES_H_

#include <memory>
#include <string>

namespace exo {

// Defines the set of actions/associations which are needed to implement a
// per-product Exo server. Product here refrs to things like "Crostini", "ArcVM"
// and "Lacross", and distinguishes from wayland's notion of a client. Each
// product may have multiple clients associated with it.
//
// TODO(b/200896773): Flesh this class out once we're clear on what things
// should be server capabilities.
class Capabilities {
 public:
  // Get a capability instance with all of the defaults.
  static std::unique_ptr<Capabilities> GetDefaultCapabilities();

  virtual ~Capabilities();

  // The path of the wayland server will be determined (partially) by its
  // security context. This process is documented in go/secure-exo-ids. All
  // sockets for the same security context will be placed in a single directory,
  // on ChromeOS that directory is "/run/wayland/<context>/". The intention is
  // that systems that need access to the wayland socket will mount their
  // security context's directory into their mount namespace, and not others'.
  //
  // The empty string refers to "no security context", only the default wayland
  // server may use it, and it is an error to spawn a non-default server without
  // a security context.
  virtual std::string GetSecurityContext() const = 0;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_CAPABILITIES_H_
