// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TEST_THEME_PROVIDER_H_
#define CHROME_TEST_BASE_TEST_THEME_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/theme_provider.h"

// Test ui::ThemeProvider implementation.
class TestThemeProvider : public ui::ThemeProvider {
 public:
  TestThemeProvider();
  ~TestThemeProvider() override;

  // ui::ThemeProvider:
  gfx::ImageSkia* GetImageSkiaNamed(int id) const override;
  color_utils::HSL GetTint(int id) const override;
  int GetDisplayProperty(int id) const override;
  bool ShouldUseNativeFrame() const override;
  bool HasCustomImage(int id) const override;
  base::RefCountedMemory* GetRawData(
      int id,
      ui::ResourceScaleFactor scale_factor) const override;

  // Set a custom color.
  void SetColor(int id, SkColor color);

 private:
  base::flat_map<int, SkColor> colors_;
};

#endif  // CHROME_TEST_BASE_TEST_THEME_PROVIDER_H_
