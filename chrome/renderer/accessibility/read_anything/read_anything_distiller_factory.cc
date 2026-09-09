// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_distiller_factory.h"

#include <memory>
#include <utility>

#include "base/notreached.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_app_model.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_distiller.h"
#include "chrome/renderer/accessibility/read_anything/screen2x_distiller.h"

ReadAnythingDistillerFactory::ReadAnythingDistillerFactory(
    content::RenderFrame* render_frame,
    ScreenAIReadinessCallback is_screen_ai_ready_callback)
    : render_frame_(render_frame),
      is_screen_ai_ready_callback_(std::move(is_screen_ai_ready_callback)) {}

ReadAnythingDistillerFactory::~ReadAnythingDistillerFactory() = default;

std::unique_ptr<ReadAnythingDistiller>
ReadAnythingDistillerFactory::CreateDistiller(
    ReadAnythingAppModel::DistillationMethod method,
    ReadAnythingDistiller::DistillationCompleteCallback on_complete_callback) {
  switch (method) {
    case ReadAnythingAppModel::DistillationMethod::kScreen2x:
      return std::make_unique<Screen2xDistiller>(
          render_frame_, is_screen_ai_ready_callback_,
          std::move(on_complete_callback));
    case ReadAnythingAppModel::DistillationMethod::kReadability:
      // TODO(crbug.com/543987370): Implement in follow-up CL for
      // ReadabilityDistiller.
      NOTREACHED();
  }
}
