// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"

#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"

namespace {

NativeBrowserFrameFactory* factory = nullptr;

}

// static
NativeBrowserFrame* NativeBrowserFrameFactory::CreateNativeBrowserFrame(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
  if (!factory)
    factory = new NativeBrowserFrameFactory;
  return factory->Create(browser_frame, browser_view);
}

// static
void NativeBrowserFrameFactory::Set(NativeBrowserFrameFactory* new_factory) {
  delete factory;
  factory = new_factory;
}
