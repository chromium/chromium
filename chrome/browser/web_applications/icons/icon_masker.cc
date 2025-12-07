// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/icons/icon_masker.h"

#include <optional>

#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace web_app {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
void MaskIconOnOs(SkBitmap input_bitmap, MaskedIconCallback masked_callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(masked_callback), std::move(input_bitmap)));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

}  // namespace web_app
