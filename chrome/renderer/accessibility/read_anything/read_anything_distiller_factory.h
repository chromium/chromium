// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_DISTILLER_FACTORY_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_DISTILLER_FACTORY_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_app_model.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_distiller.h"

namespace content {
class RenderFrame;
}  // namespace content

// Factory responsible for instantiating concrete ReadAnythingDistiller engines
// (e.g., Screen2xDistiller, ReadabilityDistiller).
//
// Encapsulates the different dependencies required by each engine (such as
// RenderFrame, ScreenAI readiness checks, or Mojo remotes) so that callers
// do not need to assemble engine-specific plumbing at creation call sites.
class ReadAnythingDistillerFactory {
 public:
  using ScreenAIReadinessCallback = base::RepeatingCallback<bool()>;

  ReadAnythingDistillerFactory(
      content::RenderFrame* render_frame,
      ScreenAIReadinessCallback is_screen_ai_ready_callback);
  virtual ~ReadAnythingDistillerFactory();
  ReadAnythingDistillerFactory(const ReadAnythingDistillerFactory&) = delete;
  ReadAnythingDistillerFactory& operator=(const ReadAnythingDistillerFactory&) =
      delete;

  // Creates and returns a ReadAnythingDistiller instance configured for the
  // requested distillation `method`.
  //
  // `on_complete_callback` is forwarded to the newly created distiller and
  // invoked whenever distillation finishes.
  virtual std::unique_ptr<ReadAnythingDistiller> CreateDistiller(
      ReadAnythingAppModel::DistillationMethod method,
      ReadAnythingDistiller::DistillationCompleteCallback on_complete_callback);

 private:
  const raw_ptr<content::RenderFrame> render_frame_;

  // Callback forwarded to Screen2xDistiller to query whether the ScreenAI
  // service in the utility process is ready for distillation.
  ScreenAIReadinessCallback is_screen_ai_ready_callback_;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_DISTILLER_FACTORY_H_
