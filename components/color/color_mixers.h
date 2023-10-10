// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLOR_COLOR_MIXERS_H_
#define COMPONENTS_COLOR_COLOR_MIXERS_H_

namespace ui {
class ColorProvider;
struct ColorProviderKey;
}  // namespace ui

namespace color {

// Adds color mixers to `provider` that provide //components colors.
void AddComponentsColorMixers(ui::ColorProvider* provider,
                              const ui::ColorProviderKey& key);

}  // namespace color

#endif  // COMPONENTS_COLOR_COLOR_MIXERS_H_
