// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/typography.h"

#include <ostream>

#include "base/check.h"
#include "base/no_destructor.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "ui/gfx/font_list.h"

namespace quick_answers {

namespace {

const gfx::FontList& GetCurrentDesignFontList() {
  static const base::NoDestructor<gfx::FontList> current_design_font_list(
      gfx::FontList({"Roboto"}, gfx::Font::NORMAL, 12,
                    gfx::Font::Weight::NORMAL));
  return *current_design_font_list;
}

constexpr int kCurrentDesignLineHeight = 20;

}  // namespace

const gfx::FontList& GetCrosAnnotation1FontList() {
  static const base::NoDestructor<gfx::FontList> annotation_1_font_list(
      gfx::FontList({"Google Sans", "Roboto"}, gfx::Font::NORMAL, 12,
                    gfx::Font::Weight::NORMAL));
  return *annotation_1_font_list;
}

int GetCrosAnnotation1LineHeight() {
  return 18;
}

const gfx::FontList& GetFirstLineFontList(Design design) {
  static const base::NoDestructor<gfx::FontList> cros_headline_1(
      gfx::FontList({"Google Sans", "Roboto"}, gfx::Font::NORMAL, 15,
                    gfx::Font::Weight::MEDIUM));

  switch (design) {
    case Design::kCurrent:
      return GetCurrentDesignFontList();
    case Design::kRefresh:
    case Design::kMagicBoost:
      // TODO(b/340629098): remove a dependency from lacros and use
      // `ash::TypographyProvider`
      // `ash::TypographyToken::kCrosHeadline1`
      return *cros_headline_1;
  }

  CHECK(false) << "Invalid design enum value provided";
}

int GetFirstLineHeight(Design design) {
  switch (design) {
    case Design::kCurrent:
      return kCurrentDesignLineHeight;
    case Design::kRefresh:
    case Design::kMagicBoost:
      // `ash::TypographyToken::kCrosHeadline1`
      return 22;
  }

  CHECK(false) << "Invalid design enum value provided";
}

const gfx::FontList& GetSecondLineFontList(Design design) {
  switch (design) {
    case Design::kCurrent:
      return GetCurrentDesignFontList();
    case Design::kRefresh:
    case Design::kMagicBoost:
      return GetCrosAnnotation1FontList();
  }

  CHECK(false) << "Invalid design enum value provided";
}

int GetSecondLineHeight(Design design) {
  switch (design) {
    case Design::kCurrent:
      return kCurrentDesignLineHeight;
    case Design::kRefresh:
    case Design::kMagicBoost:
      return GetCrosAnnotation1LineHeight();
  }

  CHECK(false) << "Invalid design enum value provided";
}

}  // namespace quick_answers
