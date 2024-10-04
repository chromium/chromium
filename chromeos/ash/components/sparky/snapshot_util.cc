// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sparky/snapshot_util.h"

#include <algorithm>

#include "ash/shell.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace sparky {

ScreenshotHandler::ScreenshotHandler() = default;

ScreenshotHandler::~ScreenshotHandler() = default;

void ScreenshotHandler::TakeScreenshot(ScreenshotDataCallback done_callback) {
  // TODO: Support multiple displays. For now, use the most
  // recently active display.
  aura::Window* root = ash::Shell::GetRootWindowForNewWindows();
  DCHECK(root);

  ui::GrabWindowSnapshotAsPNG(
      root,
      /*source_rect=*/gfx::Rect(gfx::Point(), root->bounds().size()),
      base::BindOnce(std::move(done_callback)));
}

}  // namespace sparky
