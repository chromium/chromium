// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_CAPABILITIES_H_
#define COMPONENTS_EXO_CAPABILITIES_H_

#include <memory>

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
};

}  // namespace exo

#endif  // COMPONENTS_EXO_CAPABILITIES_H_
