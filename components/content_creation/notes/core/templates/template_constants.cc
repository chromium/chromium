// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_constants.h"

#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace content_creation {
namespace {

const uint16_t k400Weight = 400;
const uint16_t k700Weight = 700;

const int kDefaultMinTextSizeSP = 14;
const int kFriendlyMinTextSizeSP = 9;
const int kDefaultMaxTextSizeSP = 48;
const int kFriendlyMaxSizeSP = 32;
const int kBiggerMaxTextSizeSP = 60;

const char kBebasNeueFontName[] = "Bebas Neue";
const char kMansalvaFontName[] = "Mansalva";
const char kRobotoCondensedFontName[] = "Roboto Condensed";
const char kRockSaltFontName[] = "Rock Salt";
const char kSourceSerifProFontName[] = "Source Serif Pro";

const ARGBColor kBlue900Color = 0xFF174EA6;
const ARGBColor kGreen50Color = 0xFFE6F4EA;
const ARGBColor kGreen900Color = 0xFF0D652D;
const ARGBColor kGrey300Color = 0xFFDADCE0;
const ARGBColor kGrey900Color = 0xFF202124;
const ARGBColor kDarkRedColor = 0xFFCC4A2D;
const ARGBColor kYellow400Color = 0xFFFCC934;

const ARGBColor kLightYellowColor = 0xFFFCEF94;

const ARGBColor kWhiteColor = 0xFFFFFFFF;
const ARGBColor kBlackColor = 0xFF000000;

// White color with 0.7 opacity.
const ARGBColor kSlightlyTransparentWhiteColor = 0xB3FFFFFF;

// White color with 0.2 opacity.
const ARGBColor kTransparentWhiteColor = 0x33FFFFFF;

// Black color with 0.8 opacity.
const ARGBColor kSlightlyTransparentBlackColor = 0xCC000000;

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

FooterStyle CreateGroovyDreamyTemplateFooterStyle() {
  return FooterStyle(
      /*text_color=*/kWhiteColor,
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
                /*all_caps=*/false, TextAlignment::kStart,
                kDefaultMinTextSizeSP, kDefaultMaxTextSizeSP),
      /*footer_style=*/CreateDarkBackgroundFooterStyle());
}

NoteTemplate GetFriendlyTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kFriendly,
      l10n_util::GetStringUTF8(
          IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_FRIENDLY),
      Background(
          /*image_url=*/"https://www.gstatic.com/chrome/content/"
                        "webnotes/backgrounds/FriendlyBackground@2x.png"),
      TextStyle(kRockSaltFontName,
                /*font_color=*/kGrey900Color, k400Weight,
                /*all_caps=*/false, TextAlignment::kStart,
                kFriendlyMinTextSizeSP, kFriendlyMaxSizeSP),
      /*footer_style=*/CreateLightBackgroundFooterStyle());
}

NoteTemplate GetFreshTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kFresh,
      l10n_util::GetStringUTF8(IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_FRESH),
      Background(/*color=*/kGreen50Color),
      TextStyle(kSourceSerifProFontName,
                /*font_color=*/kGreen900Color, k400Weight,
                /*all_caps=*/false, TextAlignment::kStart,
                kDefaultMinTextSizeSP, kDefaultMaxTextSizeSP),
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
                /*all_caps=*/true, TextAlignment::kStart, kDefaultMinTextSizeSP,
                kDefaultMaxTextSizeSP),
      /*footer_style=*/CreateLightBackgroundFooterStyle());
}

NoteTemplate GetImpactfulTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kImpactful,
      l10n_util::GetStringUTF8(
          IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_IMPACTFUL),
      Background(/*color=*/kGrey300Color),
      TextStyle(kBebasNeueFontName,
                /*font_color=*/kBlackColor, k400Weight,
                /*all_caps=*/true, TextAlignment::kCenter,
                kDefaultMinTextSizeSP, kBiggerMaxTextSizeSP,
                /*highlight_color=*/kWhiteColor, HighlightStyle::kHalf),
      /*footer_style=*/CreateLightBackgroundFooterStyle());
}

NoteTemplate GetLovelyTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kLovely,
      l10n_util::GetStringUTF8(IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_LOVELY),
      /*main_background=*/
      Background(/*colors=*/{0xFFCEF9FF, 0xFFF1DFFF},
                 LinearGradientDirection::kTopRightToBottomLeft),
      /*content_background=*/Background(/*color=*/kWhiteColor),
      TextStyle(kSourceSerifProFontName,
                /*font_color=*/kBlackColor, k400Weight,
                /*all_caps=*/false, TextAlignment::kCenter,
                kDefaultMinTextSizeSP, kDefaultMaxTextSizeSP),
      /*footer_style=*/CreateLightBackgroundFooterStyle());
}

NoteTemplate GetGroovyTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kGroovy,
      l10n_util::GetStringUTF8(IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_GROOVY),
      Background(/*color=*/kDarkRedColor),
      TextStyle(kBebasNeueFontName,
                /*font_color=*/kYellow400Color, k400Weight,
                /*all_caps=*/true, TextAlignment::kStart, kDefaultMinTextSizeSP,
                kBiggerMaxTextSizeSP,
                /*highlight_color=*/kBlue900Color, HighlightStyle::kFull),
      /*footer_style=*/CreateGroovyDreamyTemplateFooterStyle());
}

NoteTemplate GetMonochromeTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kMonochrome,
      l10n_util::GetStringUTF8(
          IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_MONOCHROME),
      Background(/*color=*/kBlackColor),
      TextStyle(kBebasNeueFontName,
                /*font_color=*/kWhiteColor, k400Weight,
                /*all_caps=*/true, TextAlignment::kCenter,
                kDefaultMinTextSizeSP, kBiggerMaxTextSizeSP),
      /*footer_style=*/CreateDarkBackgroundFooterStyle());
}

NoteTemplate GetBoldTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kBold,
      l10n_util::GetStringUTF8(IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_BOLD),
      Background(/*color=*/kWhiteColor),
      TextStyle(kBebasNeueFontName,
                /*font_color=*/kBlackColor, k400Weight,
                /*all_caps=*/true, TextAlignment::kCenter,
                kDefaultMinTextSizeSP, kBiggerMaxTextSizeSP,
                /*highlight_color=*/kLightYellowColor, HighlightStyle::kHalf),
      /*footer_style=*/CreateLightBackgroundFooterStyle());
}

NoteTemplate GetDreamyTemplate() {
  return NoteTemplate(
      /*id=*/NoteTemplateIds::kDreamy,
      l10n_util::GetStringUTF8(IDS_CONTENT_CREATION_NOTE_TEMPLATE_NAME_DREAMY),
      Background(/*colors=*/{0xFFDB80B8, 0xFF7462B8},
                 LinearGradientDirection::kTopToBottom),
      TextStyle(kMansalvaFontName,
                /*font_color=*/kWhiteColor, k400Weight,
                /*all_caps=*/false, TextAlignment::kStart,
                kDefaultMinTextSizeSP, kDefaultMaxTextSizeSP),
      /*footer_style=*/CreateGroovyDreamyTemplateFooterStyle());
}

}  // namespace content_creation
