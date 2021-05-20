// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_constants.h"

#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace content_creation {
namespace {

const uint16_t k400Weight = 400;
const uint16_t k700Weight = 700;

const char kBebasNeueFontName[] = "Bebas Neue";
const char kMansalvaFontName[] = "Mansalva";
const char kRobotoCondensedFontName[] = "Roboto Condensed";
const char kSourceSerifProFontName[] = "Source Serif Pro";

const ARGBColor kGreen50Color = 0xFFE6F4EA;
const ARGBColor kGreen900Color = 0xFF0D652D;
const ARGBColor kGrey200Color = 0xFFE8EAED;
const ARGBColor kGrey900Color = 0xFF202124;
const ARGBColor kYellow400Color = 0xFFFCC934;

const ARGBColor kWhiteColor = 0xFFFFFFFF;
const ARGBColor kBlackColor = 0xFF000000;

// White color with 0.7 opacity.
const ARGBColor kSlightlyTransparentWhiteColor = 0xB3FFFFFF;

// White color with 0.2 opacity.
const ARGBColor kTransparentWhiteColor = 0x33FFFFFF;

// Black color with 0.5 opacity.
const ARGBColor kSlightlyTransparentBlackColor = 0x80000000;

// Black color with 0.1 opacity.
const ARGBColor kTransparentBlackColor = 0x1A000000;

FooterStyle CreateLightBackgroundFooterStyle() {
  return FooterStyle(
      /*text_color=*/kSlightlyTransparentBlackColor,
      /*logo_color=*/kTransparentBlackColor);
}

FooterStyle CreateDarkBackgroundFooterStyle() {
  return FooterStyle(
      /*text_color=*/kSlightlyTransparentWhiteColor,
      /*logo_color=*/kTransparentWhiteColor);
}

}  // namespace

NoteTemplate GetClassicTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kClassic,
      l10n_util::GetStringUTF8(IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_CLASSIC),
      Background(/*color=*/kGrey900Color),
      TextStyle(kSourceSerifProFontName,
                /*font_color=*/kWhiteColor, k700Weight,
                /*all_caps=*/false, TextAlignment::kStart),
      /*footer_style=*/CreateDarkBackgroundFooterStyle());
}

NoteTemplate GetFreshTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kFresh,
      l10n_util::GetStringUTF8(IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_FRESH),
      Background(/*color=*/kGreen50Color),
      TextStyle(kSourceSerifProFontName,
                /*font_color=*/kGreen900Color, k400Weight,
                /*all_caps=*/false, TextAlignment::kStart),
      /*footer_style=*/CreateLightBackgroundFooterStyle());
}

NoteTemplate GetPowerfulTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kPowerful,
      l10n_util::GetStringUTF8(
          IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_POWERFUL),
      Background(/*color=*/kYellow400Color),
      TextStyle(kRobotoCondensedFontName,
                /*font_color=*/kBlackColor, k400Weight,
                /*all_caps=*/true, TextAlignment::kStart),
      /*footer_style=*/CreateLightBackgroundFooterStyle());
}

NoteTemplate GetImpactfulTemplate() {
  // TODO(crbug.com/1194168): Add text background color.
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kImpactful,
      l10n_util::GetStringUTF8(
          IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_IMPACTFUL),
      Background(/*color=*/kGrey200Color),
      TextStyle(kBebasNeueFontName,
                /*font_color=*/kBlackColor, k400Weight,
                /*all_caps=*/true, TextAlignment::kCenter),
      /*footer_style=*/CreateLightBackgroundFooterStyle());
}

NoteTemplate GetMonochromeTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kMonochrome,
      l10n_util::GetStringUTF8(
          IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_MONOCHROME),
      Background(/*color=*/kBlackColor),
      TextStyle(kBebasNeueFontName,
                /*font_color=*/kWhiteColor, k400Weight,
                /*all_caps=*/true, TextAlignment::kCenter),
      /*footer_style=*/CreateDarkBackgroundFooterStyle());
}

NoteTemplate GetBoldTemplate() {
  // TODO(crbug.com/1194168): Add text background color.
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kBold,
      l10n_util::GetStringUTF8(IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_BOLD),
      Background(/*color=*/kWhiteColor),
      TextStyle(kBebasNeueFontName,
                /*font_color=*/kBlackColor, k400Weight,
                /*all_caps=*/true, TextAlignment::kCenter),
      /*footer_style=*/CreateLightBackgroundFooterStyle());
}

NoteTemplate GetDreamyTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kDreamy,
      l10n_util::GetStringUTF8(IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_DREAMY),
      Background(/*colors=*/{0xFFDB80B8, 0xFFF39FD3, 0xFFA89CED},
                 LinearGradientDirection::kTopToBottom),
      TextStyle(kMansalvaFontName,
                /*font_color=*/kWhiteColor, k400Weight,
                /*all_caps=*/false, TextAlignment::kStart),
      /*footer_style=*/CreateLightBackgroundFooterStyle());
}

}  // namespace content_creation
