// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CCO_MULTILINE_DETECTOR_H_
#define CHROME_RENDERER_CCO_MULTILINE_DETECTOR_H_

#include "content/public/renderer/render_frame_observer.h"

class MultilineDetector : public content::RenderFrameObserver {
 public:
  static void InstallIfNecessary(content::RenderFrame* render_frame);

  // content::RenderFrameObserver overrides
  void OnDestruct() override;
  void FocusedElementChanged(const blink::WebElement& element) override;

 private:
  explicit MultilineDetector(content::RenderFrame* render_frame);
  ~MultilineDetector() override;
};

#endif  // CHROME_RENDERER_MULTILINE_DETECTOR_H_
