// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_URL_TEXT_H_
#define CHROME_BROWSER_VR_ELEMENTS_URL_TEXT_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/omnibox_formatting.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/render_text.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace vr {

class VR_UI_EXPORT UrlText : public Text {
 public:
  explicit UrlText(float font_height_dmm);
  ~UrlText() override;

  void SetUrl(const GURL& url);
  void SetColor(SkColor color) override;
  void SetEmphasizedColor(SkColor color);
  void SetDeemphasizedColor(SkColor color);

 private:
  void UpdateText();

  void OnRenderTextCreated(gfx::RenderText* render_text);
  void OnRenderTextRendered(const gfx::RenderText& render_text,
                            SkCanvas* canvas);

  GURL gurl_;
  url::Parsed url_parsed_;
  SkColor emphasized_color_ = SK_ColorBLACK;
  SkColor deemphasized_color_ = SK_ColorBLACK;
  ElisionParameters elision_parameters_;
  float font_height_dmm_ = 0.f;

  DISALLOW_COPY_AND_ASSIGN(UrlText);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_URL_TEXT_H_
