// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_TEXTURE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

class SkCanvas;

namespace gfx {

class PointF;

}  // namespace gfx

namespace vr {

class UiTexture {
 public:
  UiTexture();

  UiTexture(const UiTexture&) = delete;
  UiTexture& operator=(const UiTexture&) = delete;

  virtual ~UiTexture();

  void DrawTexture(SkCanvas* canvas, const gfx::Size& texture_size);

  // Marks the texture as drawn, when there isn't anything to draw.  For
  // example, a text element with no text in it.
  void DrawEmptyTexture();

  virtual void Draw(SkCanvas* canvas, const gfx::Size& texture_size) = 0;

  virtual bool LocalHitTest(const gfx::PointF& point) const;

  bool measured() const { return measured_; }
  bool dirty() const { return dirty_; }

  void OnInitialized();

  // Foreground and background colors are used pervasively in textures, but more
  // element-specific colors should be set on the appropriate class.
  void SetForegroundColor(SkColor color);
  void SetBackgroundColor(SkColor color);

 protected:
  template <typename T>
  void SetAndDirty(T* target, const T& value) {
    if (*target != value) {
      set_dirty();
    }
    *target = value;
  }

  template <typename T>
  void SetAndDirty(raw_ptr<const T>* target, const T* value) {
    if (*target != value) {
      set_dirty();
    }
    *target = value;
  }

  void set_dirty() {
    measured_ = false;
    dirty_ = true;
  }

  // Textures that depend on measurement to draw must call this when they
  // complete measurement work.
  void set_measured() { measured_ = true; }

  SkColor foreground_color() const;
  SkColor background_color() const;

 private:
  bool measured_ = false;
  bool dirty_ = true;
  std::optional<SkColor> foreground_color_;
  std::optional<SkColor> background_color_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_TEXTURE_H_
