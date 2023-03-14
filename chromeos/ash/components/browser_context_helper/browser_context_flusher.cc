// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/browser_context_helper/browser_context_flusher.h"

#include "base/check.h"
#include "chromeos/ash/components/browser_context_helper/file_flusher.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace {

BrowserContextFlusher* instance = nullptr;

}  // namespace

BrowserContextFlusher::BrowserContextFlusher() {
  DCHECK(!instance);
  instance = this;
}

BrowserContextFlusher::~BrowserContextFlusher() {
  DCHECK_EQ(instance, this);
  instance = nullptr;
}

// static
BrowserContextFlusher* BrowserContextFlusher::Get() {
  return instance;
}

void BrowserContextFlusher::ScheduleFlush(
    content::BrowserContext* browser_context) {
  if (!flusher_) {
    flusher_ = std::make_unique<FileFlusher>();
  }

  // Flushes files directly under browser_context's path since these are the
  // critical ones.
  flusher_->RequestFlush(browser_context->GetPath(), /*recursive=*/false,
                         base::OnceClosure());
}

}  // namespace ash
