// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_SCOPED_WL_H_
#define COMPONENTS_EXO_WAYLAND_SCOPED_WL_H_

extern "C" {
struct wl_display;
}

namespace exo {
namespace wayland {

struct WlDisplayDeleter {
  void operator()(wl_display* display) const;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_SCOPED_WL_H_
