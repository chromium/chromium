// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_linux.h"

BrowserFrameViewLinux::BrowserFrameViewLinux(
    BrowserFrame* frame,
    BrowserView* browser_view,
    BrowserFrameViewLayoutLinux* layout)
    : OpaqueBrowserFrameView(frame, browser_view, layout) {}

BrowserFrameViewLinux::~BrowserFrameViewLinux() = default;
