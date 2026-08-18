// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dictation/waveform_view_button.h"

#include <memory>

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

namespace dictation {

WaveformViewButton::WaveformViewButton(bool full_size, PressedCallback callback)
    : views::Button(std::move(callback)) {
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_DICTATION_ACCNAME_OVERLAY_WAVEFORM_BUTTON));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto waveform_view = std::make_unique<WaveformView>(full_size);
  waveform_view->SetCanProcessEventsWithinSubtree(false);
  waveform_view_ = AddChildView(std::move(waveform_view));
}

WaveformViewButton::~WaveformViewButton() = default;

BEGIN_METADATA(WaveformViewButton)
END_METADATA

}  // namespace dictation
