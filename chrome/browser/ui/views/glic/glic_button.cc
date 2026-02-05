// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/glic/glic_button.h"

#include "base/notimplemented.h"

namespace glic {

template <typename T>
  requires std::derived_from<T, views::LabelButton>
void GlicButton<T>::Init(const std::u16string& tooltip) {}

template <typename T>
  requires std::derived_from<T, views::LabelButton>
GlicButton<T>::~GlicButton() = default;

template <typename T>
  requires std::derived_from<T, views::LabelButton>
void GlicButton<T>::SetDropToAttachIndicator(bool indicate) {
  // TODO(crbug.com/454112198): Legacy "attached mode" code, clean this up.
  if (indicate) {
    SetBackgroundFrameActiveColorId(ui::kColorSysStateHeaderHover);
  } else {
    SetBackgroundFrameActiveColorId(kColorNewTabButtonCRBackgroundFrameActive);
  }
}

template <typename T>
  requires std::derived_from<T, views::LabelButton>
gfx::Rect GlicButton<T>::GetBoundsWithInset() const {
  // TODO(crbug.com/454112198): Legacy "attached mode" code, clean this up.
  gfx::Rect bounds = this->GetBoundsInScreen();
  bounds.Inset(this->GetInsets());
  return bounds;
}

}  // namespace glic
