// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/browser/window.h"

#import <Cocoa/Cocoa.h>
#include <map>

#include "base/no_destructor.h"

namespace remote_cocoa {
namespace {

using NativeWindowMap = std::map<gfx::NativeWindow, ScopedNativeWindowMapping*>;
NativeWindowMap& GetMap() {
  static base::NoDestructor<NativeWindowMap> map;
  return *map.get();
}

ScopedNativeWindowMapping* GetMapping(gfx::NativeWindow native_window) {
  auto found = GetMap().find(native_window);
  if (found == GetMap().end())
    return nullptr;
  return found->second;
}

}  // namespace

ScopedNativeWindowMapping::ScopedNativeWindowMapping(
    gfx::NativeWindow native_window,
    ApplicationHost* app_host,
    NativeWidgetNSWindowBridge* in_process_window_bridge,
    mojom::NativeWidgetNSWindow* mojo_interface)
    : native_window_(native_window),
      application_host_(app_host),
      in_process_window_bridge_(in_process_window_bridge),
      mojo_interface_(mojo_interface) {
  GetMap().insert(std::make_pair(native_window, this));
}

ScopedNativeWindowMapping::~ScopedNativeWindowMapping() {
  GetMap().erase(native_window_);
}

bool IsWindowRemote(gfx::NativeWindow gfx_window) {
  auto* scoped_mapping = GetMapping(gfx_window);
  if (scoped_mapping)
    return !scoped_mapping->in_process_window_bridge();
  return false;
}

ApplicationHost* GetWindowApplicationHost(gfx::NativeWindow gfx_window) {
  auto* scoped_mapping = GetMapping(gfx_window);
  if (scoped_mapping)
    return scoped_mapping->application_host();
  return nullptr;
}

mojom::NativeWidgetNSWindow* GetWindowMojoInterface(
    gfx::NativeWindow gfx_window) {
  auto* scoped_mapping = GetMapping(gfx_window);
  if (scoped_mapping)
    return scoped_mapping->mojo_interface();
  return nullptr;
}

NSWindow* CreateInProcessTransparentClone(gfx::NativeWindow remote_window) {
  DCHECK(IsWindowRemote(remote_window));
  NSWindow* window = [[NSWindow alloc]
      initWithContentRect:[remote_window.GetNativeNSWindow() frame]
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  [window setAlphaValue:0];
  [window makeKeyAndOrderFront:nil];
  [window setLevel:NSModalPanelWindowLevel];
  return window;
}

}  // namespace remote_cocoa
