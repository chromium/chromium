// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/warm_compositor.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "components/paint_preview/browser/compositor_utils.h"

namespace paint_preview {

WarmCompositor::WarmCompositor()
    : compositor_service_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}
WarmCompositor::~WarmCompositor() = default;

// static
WarmCompositor* WarmCompositor::GetInstance() {
  return base::Singleton<WarmCompositor,
                         base::LeakySingletonTraits<WarmCompositor>>::get();
}

void WarmCompositor::WarmupCompositor() {
  if (compositor_service_)
    return;

  compositor_service_ = StartCompositorService(base::BindOnce(
      &WarmCompositor::OnDisconnect, weak_ptr_factory_.GetWeakPtr()));
}

bool WarmCompositor::StopCompositor() {
  if (!compositor_service_)
    return false;

  compositor_service_.reset();
  return true;
}

std::unique_ptr<PaintPreviewCompositorService, base::OnTaskRunnerDeleter>
WarmCompositor::GetOrStartCompositorService(
    base::OnceClosure disconnect_handler) {
  if (!compositor_service_)
    return StartCompositorService(std::move(disconnect_handler));

  compositor_service_->SetDisconnectHandler(std::move(disconnect_handler));
  return std::move(compositor_service_);
}

void WarmCompositor::OnDisconnect() {
  compositor_service_.reset();
}

}  // namespace paint_preview
