// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PEPPER_HELPER_H_
#define CHROME_RENDERER_PEPPER_PEPPER_HELPER_H_

#include "content/public/renderer/render_frame_observer.h"

// This class listens for Pepper creation events from the RenderFrame and
// attaches the parts required for Chrome-specific plugin support.
class PepperHelper : public content::RenderFrameObserver {
 public:
  explicit PepperHelper(content::RenderFrame* render_frame);

  PepperHelper(const PepperHelper&) = delete;
  PepperHelper& operator=(const PepperHelper&) = delete;

  ~PepperHelper() override;

  // RenderFrameObserver.
  void DidCreatePepperPlugin(content::RendererPpapiHost* host) override;
  void OnDestruct() override;
};

#endif  // CHROME_RENDERER_PEPPER_PEPPER_HELPER_H_
