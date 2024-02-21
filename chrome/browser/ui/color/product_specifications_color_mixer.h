// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_PRODUCT_SPECIFICATIONS_COLOR_MIXER_H_
#define CHROME_BROWSER_UI_COLOR_PRODUCT_SPECIFICATIONS_COLOR_MIXER_H_

namespace ui {
class ColorProvider;
struct ColorProviderKey;
}  // namespace ui

// Adds a color mixer that contains recipes for product specifications colors to
// `provider` with `key`.
void AddProductSpecificationsColorMixer(ui::ColorProvider* provider,
                                        const ui::ColorProviderKey& key);

#endif  // CHROME_BROWSER_UI_COLOR_PRODUCT_SPECIFICATIONS_COLOR_MIXER_H_
