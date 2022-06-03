// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/presentation_receiver_window.h"

#include <memory>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/media_router/presentation_receiver_window_delegate.h"
#include "chrome/browser/ui/views/media_router/presentation_receiver_window_frame.h"
#include "chrome/browser/ui/views/media_router/presentation_receiver_window_view.h"
#include "content/public/browser/web_contents.h"

// static
PresentationReceiverWindow* PresentationReceiverWindow::Create(
    PresentationReceiverWindowDelegate* delegate,
    const gfx::Rect& bounds) {
  DCHECK(delegate);
  DCHECK(delegate->web_contents());
  auto* frame = new PresentationReceiverWindowFrame(Profile::FromBrowserContext(
      delegate->web_contents()->GetBrowserContext()));
  auto view = std::make_unique<PresentationReceiverWindowView>(frame, delegate);
  auto* view_raw = view.get();
  frame->InitReceiverFrame(std::move(view), bounds);
  view_raw->Init();
  return view_raw;
}
