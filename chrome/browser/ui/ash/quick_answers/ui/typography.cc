// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_answers/ui/typography.h"

#include <ostream>

#include "ash/style/typography.h"
#include "base/check.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
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

const gfx::FontList GetFirstLineFontList(Design design) {
  switch (design) {
    case Design::kCurrent:
      return GetCurrentDesignFontList();
    case Design::kRefresh:
    case Design::kMagicBoost:
      return ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosHeadline1);
  }

  NOTREACHED() << "Invalid design enum value provided";
}

int GetFirstLineHeight(Design design) {
  switch (design) {
    case Design::kCurrent:
      return kCurrentDesignLineHeight;
    case Design::kRefresh:
    case Design::kMagicBoost:
      return ash::TypographyProvider::Get()->ResolveLineHeight(
          ash::TypographyToken::kCrosHeadline1);
  }

  NOTREACHED() << "Invalid design enum value provided";
}

const gfx::FontList GetSecondLineFontList(Design design) {
  switch (design) {
    case Design::kCurrent:
      return GetCurrentDesignFontList();
    case Design::kRefresh:
    case Design::kMagicBoost:
      return ash::TypographyProvider::Get()->ResolveTypographyToken(
          ash::TypographyToken::kCrosAnnotation1);
  }

  NOTREACHED() << "Invalid design enum value provided";
}

int GetSecondLineHeight(Design design) {
  switch (design) {
    case Design::kCurrent:
      return kCurrentDesignLineHeight;
    case Design::kRefresh:
    case Design::kMagicBoost:
      return ash::TypographyProvider::Get()->ResolveLineHeight(
          ash::TypographyToken::kCrosAnnotation1);
  }

  NOTREACHED() << "Invalid design enum value provided";
}

}  // namespace quick_answers
