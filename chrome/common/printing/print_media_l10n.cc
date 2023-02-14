// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/printing/print_media_l10n.h"

#include <string>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/i18n/string_compare.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "components/strings/grit/components_strings.h"
#include "printing/backend/print_backend_utils.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace printing {

namespace {

base::StringPiece StandardNameForSize(const std::string& vendor_id) {
  // Mapping from dimensions to the standard IPP media name of the same size.
  // This is nearly the inverse of `media_map` below except it doesn't have the
  // entries marked as duplicate.
  static constexpr auto kSizeMap =
      base::MakeFixedFlatMap<base::StringPiece, base::StringPiece>({
          {"28x40in", "asme_f_28x40in"},
          {"1189x1682mm", "iso_2a0_1189x1682mm"},
          {"841x1189mm", "iso_a0_841x1189mm"},
          {"1189x2523mm", "iso_a0x3_1189x2523mm"},
          {"26x37mm", "iso_a10_26x37mm"},
          {"594x841mm", "iso_a1_594x841mm"},
          {"841x1783mm", "iso_a1x3_841x1783mm"},
          {"841x2378mm", "iso_a1x4_841x2378mm"},
          {"420x594mm", "iso_a2_420x594mm"},
          {"594x1261mm", "iso_a2x3_594x1261mm"},
          {"594x1682mm", "iso_a2x4_594x1682mm"},
          {"594x2102mm", "iso_a2x5_594x2102mm"},
          {"322x445mm", "iso_a3-extra_322x445mm"},
          {"297x420mm", "iso_a3_297x420mm"},
          {"420x891mm", "iso_a3x3_420x891mm"},
          {"420x1189mm", "iso_a3x4_420x1189mm"},
          {"420x1486mm", "iso_a3x5_420x1486mm"},
          {"420x1783mm", "iso_a3x6_420x1783mm"},
          {"420x2080mm", "iso_a3x7_420x2080mm"},
          {"235.5x322.3mm", "iso_a4-extra_235.5x322.3mm"},
          {"225x297mm", "iso_a4-tab_225x297mm"},
          {"210x297mm", "iso_a4_210x297mm"},
          {"297x630mm", "iso_a4x3_297x630mm"},
          {"297x841mm", "iso_a4x4_297x841mm"},
          {"297x1051mm", "iso_a4x5_297x1051mm"},
          {"297x1261mm", "iso_a4x6_297x1261mm"},
          {"297x1471mm", "iso_a4x7_297x1471mm"},
          {"297x1682mm", "iso_a4x8_297x1682mm"},
          {"297x1892mm", "iso_a4x9_297x1892mm"},
          {"174x235mm", "iso_a5-extra_174x235mm"},
          {"148x210mm", "iso_a5_148x210mm"},
          {"105x148mm", "iso_a6_105x148mm"},
          {"74x105mm", "iso_a7_74x105mm"},
          {"52x74mm", "iso_a8_52x74mm"},
          {"37x52mm", "iso_a9_37x52mm"},
          {"1000x1414mm", "iso_b0_1000x1414mm"},
          {"31x44mm", "iso_b10_31x44mm"},
          {"707x1000mm", "iso_b1_707x1000mm"},
          {"500x707mm", "iso_b2_500x707mm"},
          {"353x500mm", "iso_b3_353x500mm"},
          {"250x353mm", "iso_b4_250x353mm"},
          {"201x276mm", "iso_b5-extra_201x276mm"},
          {"176x250mm", "iso_b5_176x250mm"},
          {"125x176mm", "iso_b6_125x176mm"},
          {"125x324mm", "iso_b6c4_125x324mm"},
          {"88x125mm", "iso_b7_88x125mm"},
          {"62x88mm", "iso_b8_62x88mm"},
          {"44x62mm", "iso_b9_44x62mm"},
          {"917x1297mm", "iso_c0_917x1297mm"},
          {"28x40mm", "iso_c10_28x40mm"},
          {"648x917mm", "iso_c1_648x917mm"},
          {"458x648mm", "iso_c2_458x648mm"},
          {"324x458mm", "iso_c3_324x458mm"},
          {"229x324mm", "iso_c4_229x324mm"},
          {"162x229mm", "iso_c5_162x229mm"},
          {"114x162mm", "iso_c6_114x162mm"},
          {"114x229mm", "iso_c6c5_114x229mm"},
          {"81x114mm", "iso_c7_81x114mm"},
          {"81x162mm", "iso_c7c6_81x162mm"},
          {"57x81mm", "iso_c8_57x81mm"},
          {"40x57mm", "iso_c9_40x57mm"},
          {"110x220mm", "iso_dl_110x220mm"},
          {"53.98x85.6mm", "iso_id-1_53.98x85.6mm"},
          {"860x1220mm", "iso_ra0_860x1220mm"},
          {"610x860mm", "iso_ra1_610x860mm"},
          {"430x610mm", "iso_ra2_430x610mm"},
          {"305x430mm", "iso_ra3_305x430mm"},
          {"215x305mm", "iso_ra4_215x305mm"},
          {"900x1280mm", "iso_sra0_900x1280mm"},
          {"640x900mm", "iso_sra1_640x900mm"},
          {"450x640mm", "iso_sra2_450x640mm"},
          {"320x450mm", "iso_sra3_320x450mm"},
          {"225x320mm", "iso_sra4_225x320mm"},
          {"1030x1456mm", "jis_b0_1030x1456mm"},
          {"728x1030mm", "jis_b1_728x1030mm"},
          {"515x728mm", "jis_b2_515x728mm"},
          {"364x515mm", "jis_b3_364x515mm"},
          {"257x364mm", "jis_b4_257x364mm"},
          {"182x257mm", "jis_b5_182x257mm"},
          {"128x182mm", "jis_b6_128x182mm"},
          {"91x128mm", "jis_b7_91x128mm"},
          {"64x91mm", "jis_b8_64x91mm"},
          {"45x64mm", "jis_b9_45x64mm"},
          {"32x45mm", "jis_b10_32x45mm"},
          {"216x330mm", "jis_exec_216x330mm"},
          {"111.1x146mm", "jpn_chou2_111.1x146mm"},
          {"120x235mm", "jpn_chou3_120x235mm"},
          {"90x205mm", "jpn_chou4_90x205mm"},
          {"90x225mm", "jpn_chou40_90x225mm"},
          {"100x148mm", "jpn_hagaki_100x148mm"},
          {"240x322.1mm", "jpn_kahu_240x322.1mm"},
          {"270x382mm", "jpn_kaku1_270x382mm"},
          {"240x332mm", "jpn_kaku2_240x332mm"},
          {"216x277mm", "jpn_kaku3_216x277mm"},
          {"197x267mm", "jpn_kaku4_197x267mm"},
          {"190x240mm", "jpn_kaku5_190x240mm"},
          {"142x205mm", "jpn_kaku7_142x205mm"},
          {"119x197mm", "jpn_kaku8_119x197mm"},
          {"148x200mm", "jpn_oufuku_148x200mm"},
          {"105x235mm", "jpn_you4_105x235mm"},
          {"98x190mm", "jpn_you6_98x190mm"},
          {"10x11in", "na_10x11_10x11in"},
          {"10x13in", "na_10x13_10x13in"},
          {"10x14in", "na_10x14_10x14in"},
          {"10x15in", "na_10x15_10x15in"},
          {"11x12in", "na_11x12_11x12in"},
          {"11x15in", "na_11x15_11x15in"},
          {"12x19in", "na_12x19_12x19in"},
          {"5x7in", "na_5x7_5x7in"},
          {"6x9in", "na_6x9_6x9in"},
          {"7x9in", "na_7x9_7x9in"},
          {"9x11in", "na_9x11_9x11in"},
          {"4.375x5.75in", "na_a2_4.375x5.75in"},
          {"9x12in", "na_arch-a_9x12in"},
          {"12x18in", "na_arch-b_12x18in"},
          {"18x24in", "na_arch-c_18x24in"},
          {"24x36in", "na_arch-d_24x36in"},
          {"36x48in", "na_arch-e_36x48in"},
          {"26x38in", "na_arch-e2_26x38in"},
          {"27x39in", "na_arch-e3_27x39in"},
          {"12x19.17in", "na_b-plus_12x19.17in"},
          {"6.5x9.5in", "na_c5_6.5x9.5in"},
          {"17x22in", "na_c_17x22in"},
          {"22x34in", "na_d_22x34in"},
          {"34x44in", "na_e_34x44in"},
          {"11x14in", "na_edp_11x14in"},
          {"12x14in", "na_eur-edp_12x14in"},
          {"7.25x10.5in", "na_executive_7.25x10.5in"},
          {"44x68in", "na_f_44x68in"},
          {"8.5x12in", "na_fanfold-eur_8.5x12in"},
          {"11x14.875in", "na_fanfold-us_11x14.875in"},
          {"8.5x13in", "na_foolscap_8.5x13in"},
          {"8x13in", "na_govt-legal_8x13in"},
          {"8x10in", "na_govt-letter_8x10in"},
          {"3x5in", "na_index-3x5_3x5in"},
          {"6x8in", "na_index-4x6-ext_6x8in"},
          {"4x6in", "na_index-4x6_4x6in"},
          {"5x8in", "na_index-5x8_5x8in"},
          {"5.5x8.5in", "na_invoice_5.5x8.5in"},
          {"11x17in", "na_ledger_11x17in"},
          {"9.5x15in", "na_legal-extra_9.5x15in"},
          {"8.5x14in", "na_legal_8.5x14in"},
          {"9.5x12in", "na_letter-extra_9.5x12in"},
          {"8.5x12.69in", "na_letter-plus_8.5x12.69in"},
          {"8.5x11in", "na_letter_8.5x11in"},
          {"3.875x7.5in", "na_monarch_3.875x7.5in"},
          {"3.875x8.875in", "na_number-9_3.875x8.875in"},
          {"4.125x9.5in", "na_number-10_4.125x9.5in"},
          {"4.5x10.375in", "na_number-11_4.5x10.375in"},
          {"4.75x11in", "na_number-12_4.75x11in"},
          {"5x11.5in", "na_number-14_5x11.5in"},
          {"8.5x13.4in", "na_oficio_8.5x13.4in"},
          {"3.625x6.5in", "na_personal_3.625x6.5in"},
          {"8.5x10.83in", "na_quarto_8.5x10.83in"},
          {"8.94x14in", "na_super-a_8.94x14in"},
          {"13x19in", "na_super-b_13x19in"},
          {"30x42in", "na_wide-format_30x42in"},
          {"12x16in", "oe_12x16_12x16in"},
          {"14x17in", "oe_14x17_14x17in"},
          {"18x22in", "oe_18x22_18x22in"},
          {"17x24in", "oe_a2plus_17x24in"},
          {"2x3.5in", "oe_business-card_2x3.5in"},
          {"10x12in", "oe_photo-10r_10x12in"},
          {"12x15in", "oe_photo-12r_12x15in"},
          {"14x18in", "oe_photo-14x18_14x18in"},
          {"16x20in", "oe_photo-16r_16x20in"},
          {"20x24in", "oe_photo-20r_20x24in"},
          {"22x29.5in", "oe_photo-22r_22x29.5in"},
          {"22x28in", "oe_photo-22x28_22x28in"},
          {"24x31.5in", "oe_photo-24r_24x31.5in"},
          {"24x30in", "oe_photo-24x30_24x30in"},
          {"3.5x5in", "oe_photo-l_3.5x5in"},
          {"30x40in", "oe_photo-30r_30x40in"},
          {"8x12in", "oe_photo-s8r_8x12in"},
          {"4x4in", "oe_square-photo_4x4in"},
          {"5x5in", "oe_square-photo_5x5in"},
          {"184x260mm", "om_16k_184x260mm"},
          {"195x270mm", "om_16k_195x270mm"},
          {"55x85mm", "om_business-card_55x85mm"},
          {"55x91mm", "om_business-card_55x91mm"},
          {"54x86mm", "om_card_54x86mm"},
          {"275x395mm", "om_dai-pa-kai_275x395mm"},
          {"89x119mm", "om_dsc-photo_89x119mm"},
          {"215x315mm", "om_folio-sp_215x315mm"},
          {"210x330mm", "om_folio_210x330mm"},
          {"220x220mm", "om_invite_220x220mm"},
          {"110x230mm", "om_italian_110x230mm"},
          {"198x275mm", "om_juuro-ku-kai_198x275mm"},
          {"200x300mm", "om_large-photo_200x300mm"},
          {"130x180mm", "om_medium-photo_130x180mm"},
          {"267x389mm", "om_pa-kai_267x389mm"},
          {"300x400mm", "om_photo-30x40_300x400mm"},
          {"300x450mm", "om_photo-30x45_300x450mm"},
          {"350x460mm", "om_photo-35x46_350x460mm"},
          {"400x600mm", "om_photo-40x60_400x600mm"},
          {"500x750mm", "om_photo-50x75_500x750mm"},
          {"500x760mm", "om_photo-50x76_500x760mm"},
          {"600x900mm", "om_photo-60x90_600x900mm"},
          {"100x150mm", "om_small-photo_100x150mm"},
          {"89x89mm", "om_square-photo_89x89mm"},
          {"100x200mm", "om_wide-photo_100x200mm"},
          {"146x215mm", "prc_16k_146x215mm"},
          {"102x165mm", "prc_1_102x165mm"},
          {"102x176mm", "prc_2_102x176mm"},
          {"97x151mm", "prc_32k_97x151mm"},
          {"110x208mm", "prc_4_110x208mm"},
          {"120x320mm", "prc_6_120x320mm"},
          {"160x230mm", "prc_7_160x230mm"},
          {"120x309mm", "prc_8_120x309mm"},
          {"7.75x10.75in", "roc_16k_7.75x10.75in"},
          {"10.75x15.5in", "roc_8k_10.75x15.5in"},
      });

  // The standard sizes are separated by underscore with the dimensions in the
  // last field.
  std::vector<std::string> parts = base::SplitString(
      vendor_id, "_", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() != 3) {
    return "";
  }
  auto* it = kSizeMap.find(parts.back());
  return it != kSizeMap.end() ? it->second : "";
}

// Return the localized display name and sort group of a media name specified by
// `vendor_id` if any is found - else return an empty string in the named sizes
// group. The static map contained here is intended to reach all translated
// media names - see print_media_resources.grd.
MediaSizeInfo InfoForVendorId(const std::string& vendor_id) {
  static constexpr auto kMediaMap = base::MakeFixedFlatMap<
      base::StringPiece, std::pair<int, MediaSizeGroup>>({
      {"asme_f_28x40in",
       {PRINT_PREVIEW_MEDIA_ASME_F_28X40IN, MediaSizeGroup::kSizeIn}},
      {"iso_2a0_1189x1682mm",
       {PRINT_PREVIEW_MEDIA_ISO_2A0_1189X1682MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a0_841x1189mm",
       {PRINT_PREVIEW_MEDIA_ISO_A0_841X1189MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a0x3_1189x2523mm",
       {PRINT_PREVIEW_MEDIA_ISO_A0X3_1189X2523MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a10_26x37mm",
       {PRINT_PREVIEW_MEDIA_ISO_A10_26X37MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a1_594x841mm",
       {PRINT_PREVIEW_MEDIA_ISO_A1_594X841MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a1x3_841x1783mm",
       {PRINT_PREVIEW_MEDIA_ISO_A1X3_841X1783MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a1x4_841x2378mm",
       {PRINT_PREVIEW_MEDIA_ISO_A1X4_841X2378MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a2_420x594mm",
       {PRINT_PREVIEW_MEDIA_ISO_A2_420X594MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a2x3_594x1261mm",
       {PRINT_PREVIEW_MEDIA_ISO_A2X3_594X1261MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a2x4_594x1682mm",
       {PRINT_PREVIEW_MEDIA_ISO_A2X4_594X1682MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a2x5_594x2102mm",
       {PRINT_PREVIEW_MEDIA_ISO_A2X5_594X2102MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a3-extra_322x445mm",
       {PRINT_PREVIEW_MEDIA_ISO_A3_EXTRA_322X445MM,
        MediaSizeGroup::kSizeNamed}},
      {"iso_a3_297x420mm",
       {PRINT_PREVIEW_MEDIA_ISO_A3_297X420MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a3x3_420x891mm",
       {PRINT_PREVIEW_MEDIA_ISO_A3X3_420X891MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a3x4_420x1189mm",
       {PRINT_PREVIEW_MEDIA_ISO_A3X4_420X1189MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a3x5_420x1486mm",
       {PRINT_PREVIEW_MEDIA_ISO_A3X5_420X1486MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a3x6_420x1783mm",
       {PRINT_PREVIEW_MEDIA_ISO_A3X6_420X1783MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a3x7_420x2080mm",
       {PRINT_PREVIEW_MEDIA_ISO_A3X7_420X2080MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a4-extra_235.5x322.3mm",
       {PRINT_PREVIEW_MEDIA_ISO_A4_EXTRA_235_5X322_3MM,
        MediaSizeGroup::kSizeNamed}},
      {"iso_a4-tab_225x297mm",
       {PRINT_PREVIEW_MEDIA_ISO_A4_TAB_225X297MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a4_210x297mm",
       {PRINT_PREVIEW_MEDIA_ISO_A4_210X297MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a4x3_297x630mm",
       {PRINT_PREVIEW_MEDIA_ISO_A4X3_297X630MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a4x4_297x841mm",
       {PRINT_PREVIEW_MEDIA_ISO_A4X4_297X841MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a4x5_297x1051mm",
       {PRINT_PREVIEW_MEDIA_ISO_A4X5_297X1051MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a4x6_297x1261mm",
       {PRINT_PREVIEW_MEDIA_ISO_A4X6_297X1261MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a4x7_297x1471mm",
       {PRINT_PREVIEW_MEDIA_ISO_A4X7_297X1471MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a4x8_297x1682mm",
       {PRINT_PREVIEW_MEDIA_ISO_A4X8_297X1682MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a4x9_297x1892mm",
       {PRINT_PREVIEW_MEDIA_ISO_A4X9_297X1892MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a5-extra_174x235mm",
       {PRINT_PREVIEW_MEDIA_ISO_A5_EXTRA_174X235MM,
        MediaSizeGroup::kSizeNamed}},
      {"iso_a5_148x210mm",
       {PRINT_PREVIEW_MEDIA_ISO_A5_148X210MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a6_105x148mm",
       {PRINT_PREVIEW_MEDIA_ISO_A6_105X148MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a7_74x105mm",
       {PRINT_PREVIEW_MEDIA_ISO_A7_74X105MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a8_52x74mm",
       {PRINT_PREVIEW_MEDIA_ISO_A8_52X74MM, MediaSizeGroup::kSizeNamed}},
      {"iso_a9_37x52mm",
       {PRINT_PREVIEW_MEDIA_ISO_A9_37X52MM, MediaSizeGroup::kSizeNamed}},
      {"iso_b0_1000x1414mm",
       {PRINT_PREVIEW_MEDIA_ISO_B0_1000X1414MM, MediaSizeGroup::kSizeNamed}},
      {"iso_b10_31x44mm",
       {PRINT_PREVIEW_MEDIA_ISO_B10_31X44MM, MediaSizeGroup::kSizeNamed}},
      {"iso_b1_707x1000mm",
       {PRINT_PREVIEW_MEDIA_ISO_B1_707X1000MM, MediaSizeGroup::kSizeNamed}},
      {"iso_b2_500x707mm",
       {PRINT_PREVIEW_MEDIA_ISO_B2_500X707MM, MediaSizeGroup::kSizeNamed}},
      {"iso_b3_353x500mm",
       {PRINT_PREVIEW_MEDIA_ISO_B3_353X500MM, MediaSizeGroup::kSizeNamed}},
      {"iso_b4_250x353mm",
       {PRINT_PREVIEW_MEDIA_ISO_B4_250X353MM, MediaSizeGroup::kSizeNamed}},
      {"iso_b5-extra_201x276mm",
       {PRINT_PREVIEW_MEDIA_ISO_B5_EXTRA_201X276MM,
        MediaSizeGroup::kSizeNamed}},
      {"iso_b5_176x250mm",
       {PRINT_PREVIEW_MEDIA_ISO_B5_176X250MM, MediaSizeGroup::kSizeNamed}},
      {"iso_b6_125x176mm",
       {PRINT_PREVIEW_MEDIA_ISO_B6_125X176MM, MediaSizeGroup::kSizeNamed}},
      {"iso_b6c4_125x324mm",
       {PRINT_PREVIEW_MEDIA_ISO_B6C4_125X324MM, MediaSizeGroup::kSizeNamed}},
      {"iso_b7_88x125mm",
       {PRINT_PREVIEW_MEDIA_ISO_B7_88X125MM, MediaSizeGroup::kSizeNamed}},
      {"iso_b8_62x88mm",
       {PRINT_PREVIEW_MEDIA_ISO_B8_62X88MM, MediaSizeGroup::kSizeNamed}},
      {"iso_b9_44x62mm",
       {PRINT_PREVIEW_MEDIA_ISO_B9_44X62MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c0_917x1297mm",
       {PRINT_PREVIEW_MEDIA_ISO_C0_917X1297MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c10_28x40mm",
       {PRINT_PREVIEW_MEDIA_ISO_C10_28X40MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c1_648x917mm",
       {PRINT_PREVIEW_MEDIA_ISO_C1_648X917MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c2_458x648mm",
       {PRINT_PREVIEW_MEDIA_ISO_C2_458X648MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c3_324x458mm",
       {PRINT_PREVIEW_MEDIA_ISO_C3_324X458MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c4_229x324mm",
       {PRINT_PREVIEW_MEDIA_ISO_C4_229X324MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c5_162x229mm",
       {PRINT_PREVIEW_MEDIA_ISO_C5_162X229MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c6_114x162mm",
       {PRINT_PREVIEW_MEDIA_ISO_C6_114X162MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c6c5_114x229mm",
       {PRINT_PREVIEW_MEDIA_ISO_C6C5_114X229MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c7_81x114mm",
       {PRINT_PREVIEW_MEDIA_ISO_C7_81X114MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c7c6_81x162mm",
       {PRINT_PREVIEW_MEDIA_ISO_C7C6_81X162MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c8_57x81mm",
       {PRINT_PREVIEW_MEDIA_ISO_C8_57X81MM, MediaSizeGroup::kSizeNamed}},
      {"iso_c9_40x57mm",
       {PRINT_PREVIEW_MEDIA_ISO_C9_40X57MM, MediaSizeGroup::kSizeNamed}},
      {"iso_dl_110x220mm",
       {PRINT_PREVIEW_MEDIA_ISO_DL_110X220MM, MediaSizeGroup::kSizeNamed}},
      {"iso_id-1_53.98x85.6mm",
       {PRINT_PREVIEW_MEDIA_ISO_ID_1_53_98X85_6MM, MediaSizeGroup::kSizeNamed}},
      // Duplicate of iso_b7_88x125mm.
      {"iso_id-3_88x125mm",
       {PRINT_PREVIEW_MEDIA_ISO_B7_88X125MM, MediaSizeGroup::kSizeNamed}},
      {"iso_ra0_860x1220mm",
       {PRINT_PREVIEW_MEDIA_ISO_RA0_860X1220MM, MediaSizeGroup::kSizeNamed}},
      {"iso_ra1_610x860mm",
       {PRINT_PREVIEW_MEDIA_ISO_RA1_610X860MM, MediaSizeGroup::kSizeNamed}},
      {"iso_ra2_430x610mm",
       {PRINT_PREVIEW_MEDIA_ISO_RA2_430X610MM, MediaSizeGroup::kSizeNamed}},
      {"iso_ra3_305x430mm",
       {PRINT_PREVIEW_MEDIA_ISO_RA3_305X430MM, MediaSizeGroup::kSizeNamed}},
      {"iso_ra4_215x305mm",
       {PRINT_PREVIEW_MEDIA_ISO_RA4_215X305MM, MediaSizeGroup::kSizeNamed}},
      {"iso_sra0_900x1280mm",
       {PRINT_PREVIEW_MEDIA_ISO_SRA0_900X1280MM, MediaSizeGroup::kSizeNamed}},
      {"iso_sra1_640x900mm",
       {PRINT_PREVIEW_MEDIA_ISO_SRA1_640X900MM, MediaSizeGroup::kSizeNamed}},
      {"iso_sra2_450x640mm",
       {PRINT_PREVIEW_MEDIA_ISO_SRA2_450X640MM, MediaSizeGroup::kSizeNamed}},
      {"iso_sra3_320x450mm",
       {PRINT_PREVIEW_MEDIA_ISO_SRA3_320X450MM, MediaSizeGroup::kSizeNamed}},
      {"iso_sra4_225x320mm",
       {PRINT_PREVIEW_MEDIA_ISO_SRA4_225X320MM, MediaSizeGroup::kSizeNamed}},
      {"jis_b0_1030x1456mm",
       {PRINT_PREVIEW_MEDIA_JIS_B0_1030X1456MM, MediaSizeGroup::kSizeNamed}},
      {"jis_b1_728x1030mm",
       {PRINT_PREVIEW_MEDIA_JIS_B1_728X1030MM, MediaSizeGroup::kSizeNamed}},
      {"jis_b2_515x728mm",
       {PRINT_PREVIEW_MEDIA_JIS_B2_515X728MM, MediaSizeGroup::kSizeNamed}},
      {"jis_b3_364x515mm",
       {PRINT_PREVIEW_MEDIA_JIS_B3_364X515MM, MediaSizeGroup::kSizeNamed}},
      {"jis_b4_257x364mm",
       {PRINT_PREVIEW_MEDIA_JIS_B4_257X364MM, MediaSizeGroup::kSizeNamed}},
      {"jis_b5_182x257mm",
       {PRINT_PREVIEW_MEDIA_JIS_B5_182X257MM, MediaSizeGroup::kSizeNamed}},
      {"jis_b6_128x182mm",
       {PRINT_PREVIEW_MEDIA_JIS_B6_128X182MM, MediaSizeGroup::kSizeNamed}},
      {"jis_b7_91x128mm",
       {PRINT_PREVIEW_MEDIA_JIS_B7_91X128MM, MediaSizeGroup::kSizeNamed}},
      {"jis_b8_64x91mm",
       {PRINT_PREVIEW_MEDIA_JIS_B8_64X91MM, MediaSizeGroup::kSizeNamed}},
      {"jis_b9_45x64mm",
       {PRINT_PREVIEW_MEDIA_JIS_B9_45X64MM, MediaSizeGroup::kSizeNamed}},
      {"jis_b10_32x45mm",
       {PRINT_PREVIEW_MEDIA_JIS_B10_32X45MM, MediaSizeGroup::kSizeNamed}},
      {"jis_exec_216x330mm",
       {PRINT_PREVIEW_MEDIA_JIS_EXEC_216X330MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_chou2_111.1x146mm",
       {PRINT_PREVIEW_MEDIA_JPN_CHOU2_111_1X146MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_chou3_120x235mm",
       {PRINT_PREVIEW_MEDIA_JPN_CHOU3_120X235MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_chou4_90x205mm",
       {PRINT_PREVIEW_MEDIA_JPN_CHOU4_90X205MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_chou40_90x225mm",
       {PRINT_PREVIEW_MEDIA_JPN_CHOU40_90X225MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_hagaki_100x148mm",
       {PRINT_PREVIEW_MEDIA_JPN_HAGAKI_100X148MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_kahu_240x322.1mm",
       {PRINT_PREVIEW_MEDIA_JPN_KAHU_240X322_1MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_kaku1_270x382mm",
       {PRINT_PREVIEW_MEDIA_JPN_KAKU1_270X382MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_kaku2_240x332mm",
       {PRINT_PREVIEW_MEDIA_JPN_KAKU2_240X332MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_kaku3_216x277mm",
       {PRINT_PREVIEW_MEDIA_JPN_KAKU3_216X277MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_kaku4_197x267mm",
       {PRINT_PREVIEW_MEDIA_JPN_KAKU4_197X267MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_kaku5_190x240mm",
       {PRINT_PREVIEW_MEDIA_JPN_KAKU5_190X240MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_kaku7_142x205mm",
       {PRINT_PREVIEW_MEDIA_JPN_KAKU7_142X205MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_kaku8_119x197mm",
       {PRINT_PREVIEW_MEDIA_JPN_KAKU8_119X197MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_oufuku_148x200mm",
       {PRINT_PREVIEW_MEDIA_JPN_OUFUKU_148X200MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_you4_105x235mm",
       {PRINT_PREVIEW_MEDIA_JPN_YOU4_105X235MM, MediaSizeGroup::kSizeNamed}},
      {"jpn_you6_98x190mm",
       {PRINT_PREVIEW_MEDIA_JPN_YOU6_98X190MM, MediaSizeGroup::kSizeNamed}},
      {"na_10x11_10x11in",
       {PRINT_PREVIEW_MEDIA_NA_10X11_10X11IN, MediaSizeGroup::kSizeIn}},
      {"na_10x13_10x13in",
       {PRINT_PREVIEW_MEDIA_NA_10X13_10X13IN, MediaSizeGroup::kSizeIn}},
      {"na_10x14_10x14in",
       {PRINT_PREVIEW_MEDIA_NA_10X14_10X14IN, MediaSizeGroup::kSizeIn}},
      {"na_10x15_10x15in",
       {PRINT_PREVIEW_MEDIA_NA_10X15_10X15IN, MediaSizeGroup::kSizeIn}},
      {"na_11x12_11x12in",
       {PRINT_PREVIEW_MEDIA_NA_11X12_11X12IN, MediaSizeGroup::kSizeIn}},
      {"na_11x15_11x15in",
       {PRINT_PREVIEW_MEDIA_NA_11X15_11X15IN, MediaSizeGroup::kSizeIn}},
      {"na_12x19_12x19in",
       {PRINT_PREVIEW_MEDIA_NA_12X19_12X19IN, MediaSizeGroup::kSizeIn}},
      {"na_5x7_5x7in",
       {PRINT_PREVIEW_MEDIA_NA_5X7_5X7IN, MediaSizeGroup::kSizeIn}},
      {"na_6x9_6x9in",
       {PRINT_PREVIEW_MEDIA_NA_6X9_6X9IN, MediaSizeGroup::kSizeNamed}},
      {"na_7x9_7x9in",
       {PRINT_PREVIEW_MEDIA_NA_7X9_7X9IN, MediaSizeGroup::kSizeNamed}},
      {"na_9x11_9x11in",
       {PRINT_PREVIEW_MEDIA_NA_9X11_9X11IN, MediaSizeGroup::kSizeNamed}},
      {"na_a2_4.375x5.75in",
       {PRINT_PREVIEW_MEDIA_NA_A2_4_375X5_75IN, MediaSizeGroup::kSizeNamed}},
      {"na_arch-a_9x12in",
       {PRINT_PREVIEW_MEDIA_NA_ARCH_A_9X12IN, MediaSizeGroup::kSizeNamed}},
      {"na_arch-b_12x18in",
       {PRINT_PREVIEW_MEDIA_NA_ARCH_B_12X18IN, MediaSizeGroup::kSizeIn}},
      {"na_arch-c_18x24in",
       {PRINT_PREVIEW_MEDIA_NA_ARCH_C_18X24IN, MediaSizeGroup::kSizeIn}},
      {"na_arch-d_24x36in",
       {PRINT_PREVIEW_MEDIA_NA_ARCH_D_24X36IN, MediaSizeGroup::kSizeIn}},
      {"na_arch-e_36x48in",
       {PRINT_PREVIEW_MEDIA_NA_ARCH_E_36X48IN, MediaSizeGroup::kSizeIn}},
      {"na_arch-e2_26x38in",
       {PRINT_PREVIEW_MEDIA_NA_ARCH_E2_26X38IN, MediaSizeGroup::kSizeIn}},
      {"na_arch-e3_27x39in",
       {PRINT_PREVIEW_MEDIA_NA_ARCH_E3_27X39IN, MediaSizeGroup::kSizeIn}},
      {"na_b-plus_12x19.17in",
       {PRINT_PREVIEW_MEDIA_NA_B_PLUS_12X19_17IN, MediaSizeGroup::kSizeNamed}},
      {"na_c5_6.5x9.5in",
       {PRINT_PREVIEW_MEDIA_NA_C5_6_5X9_5IN, MediaSizeGroup::kSizeNamed}},
      {"na_c_17x22in",
       {PRINT_PREVIEW_MEDIA_NA_C_17X22IN, MediaSizeGroup::kSizeIn}},
      {"na_d_22x34in",
       {PRINT_PREVIEW_MEDIA_NA_D_22X34IN, MediaSizeGroup::kSizeIn}},
      {"na_e_34x44in",
       {PRINT_PREVIEW_MEDIA_NA_E_34X44IN, MediaSizeGroup::kSizeIn}},
      {"na_edp_11x14in",
       {PRINT_PREVIEW_MEDIA_NA_EDP_11X14IN, MediaSizeGroup::kSizeNamed}},
      {"na_eur-edp_12x14in",
       {PRINT_PREVIEW_MEDIA_NA_EUR_EDP_12X14IN, MediaSizeGroup::kSizeNamed}},
      {"na_executive_7.25x10.5in",
       {PRINT_PREVIEW_MEDIA_NA_EXECUTIVE_7_25X10_5IN,
        MediaSizeGroup::kSizeNamed}},
      {"na_f_44x68in",
       {PRINT_PREVIEW_MEDIA_NA_F_44X68IN, MediaSizeGroup::kSizeIn}},
      {"na_fanfold-eur_8.5x12in",
       {PRINT_PREVIEW_MEDIA_NA_FANFOLD_EUR_8_5X12IN,
        MediaSizeGroup::kSizeNamed}},
      {"na_fanfold-us_11x14.875in",
       {PRINT_PREVIEW_MEDIA_NA_FANFOLD_US_11X14_875IN,
        MediaSizeGroup::kSizeNamed}},
      {"na_foolscap_8.5x13in",
       {PRINT_PREVIEW_MEDIA_NA_FOOLSCAP_8_5X13IN, MediaSizeGroup::kSizeNamed}},
      {"na_govt-legal_8x13in",
       {PRINT_PREVIEW_MEDIA_NA_GOVT_LEGAL_8X13IN, MediaSizeGroup::kSizeIn}},
      {"na_govt-letter_8x10in",
       {PRINT_PREVIEW_MEDIA_NA_GOVT_LETTER_8X10IN, MediaSizeGroup::kSizeIn}},
      {"na_index-3x5_3x5in",
       {PRINT_PREVIEW_MEDIA_NA_INDEX_3X5_3X5IN, MediaSizeGroup::kSizeIn}},
      {"na_index-4x6-ext_6x8in",
       {PRINT_PREVIEW_MEDIA_NA_INDEX_4X6_EXT_6X8IN, MediaSizeGroup::kSizeIn}},
      {"na_index-4x6_4x6in",
       {PRINT_PREVIEW_MEDIA_NA_INDEX_4X6_4X6IN, MediaSizeGroup::kSizeIn}},
      {"na_index-5x8_5x8in",
       {PRINT_PREVIEW_MEDIA_NA_INDEX_5X8_5X8IN, MediaSizeGroup::kSizeIn}},
      {"na_invoice_5.5x8.5in",
       {PRINT_PREVIEW_MEDIA_NA_INVOICE_5_5X8_5IN, MediaSizeGroup::kSizeNamed}},
      {"na_ledger_11x17in",
       {PRINT_PREVIEW_MEDIA_NA_LEDGER_11X17IN, MediaSizeGroup::kSizeNamed}},
      {"na_legal-extra_9.5x15in",
       {PRINT_PREVIEW_MEDIA_NA_LEGAL_EXTRA_9_5X15IN,
        MediaSizeGroup::kSizeNamed}},
      {"na_legal_8.5x14in",
       {PRINT_PREVIEW_MEDIA_NA_LEGAL_8_5X14IN, MediaSizeGroup::kSizeNamed}},
      {"na_letter-extra_9.5x12in",
       {PRINT_PREVIEW_MEDIA_NA_LETTER_EXTRA_9_5X12IN,
        MediaSizeGroup::kSizeNamed}},
      {"na_letter-plus_8.5x12.69in",
       {PRINT_PREVIEW_MEDIA_NA_LETTER_PLUS_8_5X12_69IN,
        MediaSizeGroup::kSizeNamed}},
      {"na_letter_8.5x11in",
       {PRINT_PREVIEW_MEDIA_NA_LETTER_8_5X11IN, MediaSizeGroup::kSizeNamed}},
      {"na_monarch_3.875x7.5in",
       {PRINT_PREVIEW_MEDIA_NA_MONARCH_3_875X7_5IN,
        MediaSizeGroup::kSizeNamed}},
      {"na_number-9_3.875x8.875in",
       {PRINT_PREVIEW_MEDIA_NA_NUMBER_9_3_875X8_875IN,
        MediaSizeGroup::kSizeNamed}},
      {"na_number-10_4.125x9.5in",
       {PRINT_PREVIEW_MEDIA_NA_NUMBER_10_4_125X9_5IN,
        MediaSizeGroup::kSizeNamed}},
      {"na_number-11_4.5x10.375in",
       {PRINT_PREVIEW_MEDIA_NA_NUMBER_11_4_5X10_375IN,
        MediaSizeGroup::kSizeNamed}},
      {"na_number-12_4.75x11in",
       {PRINT_PREVIEW_MEDIA_NA_NUMBER_12_4_75X11IN,
        MediaSizeGroup::kSizeNamed}},
      {"na_number-14_5x11.5in",
       {PRINT_PREVIEW_MEDIA_NA_NUMBER_14_5X11_5IN, MediaSizeGroup::kSizeNamed}},
      {"na_oficio_8.5x13.4in",
       {PRINT_PREVIEW_MEDIA_NA_OFICIO_8_5X13_4IN, MediaSizeGroup::kSizeNamed}},
      {"na_personal_3.625x6.5in",
       {PRINT_PREVIEW_MEDIA_NA_PERSONAL_3_625X6_5IN,
        MediaSizeGroup::kSizeNamed}},
      {"na_quarto_8.5x10.83in",
       {PRINT_PREVIEW_MEDIA_NA_QUARTO_8_5X10_83IN, MediaSizeGroup::kSizeNamed}},
      {"na_super-a_8.94x14in",
       {PRINT_PREVIEW_MEDIA_NA_SUPER_A_8_94X14IN, MediaSizeGroup::kSizeNamed}},
      {"na_super-b_13x19in",
       {PRINT_PREVIEW_MEDIA_NA_SUPER_B_13X19IN, MediaSizeGroup::kSizeNamed}},
      {"na_wide-format_30x42in",
       {PRINT_PREVIEW_MEDIA_NA_WIDE_FORMAT_30X42IN, MediaSizeGroup::kSizeIn}},
      {"oe_12x16_12x16in",
       {PRINT_PREVIEW_MEDIA_OE_12X16_12X16IN, MediaSizeGroup::kSizeIn}},
      {"oe_14x17_14x17in",
       {PRINT_PREVIEW_MEDIA_OE_14X17_14X17IN, MediaSizeGroup::kSizeIn}},
      {"oe_18x22_18x22in",
       {PRINT_PREVIEW_MEDIA_OE_18X22_18X22IN, MediaSizeGroup::kSizeIn}},
      {"oe_a2plus_17x24in",
       {PRINT_PREVIEW_MEDIA_OE_A2PLUS_17X24IN, MediaSizeGroup::kSizeIn}},
      {"oe_business-card_2x3.5in",
       {PRINT_PREVIEW_MEDIA_OE_BUSINESS_CARD_2X3_5IN, MediaSizeGroup::kSizeIn}},
      {"oe_photo-10r_10x12in",
       {PRINT_PREVIEW_MEDIA_OE_PHOTO_10R_10X12IN, MediaSizeGroup::kSizeIn}},
      {"oe_photo-12r_12x15in",
       {PRINT_PREVIEW_MEDIA_OE_PHOTO_12R_12X15IN, MediaSizeGroup::kSizeIn}},
      {"oe_photo-14x18_14x18in",
       {PRINT_PREVIEW_MEDIA_OE_PHOTO_14X18_14X18IN, MediaSizeGroup::kSizeIn}},
      {"oe_photo-16r_16x20in",
       {PRINT_PREVIEW_MEDIA_OE_PHOTO_16R_16X20IN, MediaSizeGroup::kSizeIn}},
      {"oe_photo-20r_20x24in",
       {PRINT_PREVIEW_MEDIA_OE_PHOTO_20R_20X24IN, MediaSizeGroup::kSizeIn}},
      {"oe_photo-22r_22x29.5in",
       {PRINT_PREVIEW_MEDIA_OE_PHOTO_22R_22X29_5IN, MediaSizeGroup::kSizeIn}},
      {"oe_photo-22x28_22x28in",
       {PRINT_PREVIEW_MEDIA_OE_PHOTO_22X28_22X28IN, MediaSizeGroup::kSizeIn}},
      {"oe_photo-24r_24x31.5in",
       {PRINT_PREVIEW_MEDIA_OE_PHOTO_24R_24X31_5IN, MediaSizeGroup::kSizeIn}},
      {"oe_photo-24x30_24x30in",
       {PRINT_PREVIEW_MEDIA_OE_PHOTO_24X30_24X30IN, MediaSizeGroup::kSizeIn}},
      {"oe_photo-l_3.5x5in",
       {PRINT_PREVIEW_MEDIA_OE_PHOTO_L_3_5X5IN, MediaSizeGroup::kSizeIn}},
      {"oe_photo-30r_30x40in",
       {PRINT_PREVIEW_MEDIA_OE_PHOTO_30R_30X40IN, MediaSizeGroup::kSizeIn}},
      {"oe_photo-s8r_8x12in",
       {PRINT_PREVIEW_MEDIA_OE_PHOTO_S8R_8X12IN, MediaSizeGroup::kSizeIn}},
      // Duplicate of na_10x15_10x15in.
      {"oe_photo-s10r_10x15in",
       {PRINT_PREVIEW_MEDIA_NA_10X15_10X15IN, MediaSizeGroup::kSizeIn}},
      {"oe_square-photo_4x4in",
       {PRINT_PREVIEW_MEDIA_OE_SQUARE_PHOTO_4X4IN, MediaSizeGroup::kSizeIn}},
      {"oe_square-photo_5x5in",
       {PRINT_PREVIEW_MEDIA_OE_SQUARE_PHOTO_5X5IN, MediaSizeGroup::kSizeIn}},
      {"om_16k_184x260mm",
       {PRINT_PREVIEW_MEDIA_OM_16K_184X260MM, MediaSizeGroup::kSizeMm}},
      {"om_16k_195x270mm",
       {PRINT_PREVIEW_MEDIA_OM_16K_195X270MM, MediaSizeGroup::kSizeMm}},
      {"om_business-card_55x85mm",
       {PRINT_PREVIEW_MEDIA_OM_BUSINESS_CARD_55X85MM, MediaSizeGroup::kSizeMm}},
      {"om_business-card_55x91mm",
       {PRINT_PREVIEW_MEDIA_OM_BUSINESS_CARD_55X91MM, MediaSizeGroup::kSizeMm}},
      {"om_card_54x86mm",
       {PRINT_PREVIEW_MEDIA_OM_CARD_54X86MM, MediaSizeGroup::kSizeMm}},
      {"om_dai-pa-kai_275x395mm",
       {PRINT_PREVIEW_MEDIA_OM_DAI_PA_KAI_275X395MM, MediaSizeGroup::kSizeMm}},
      {"om_dsc-photo_89x119mm",
       {PRINT_PREVIEW_MEDIA_OM_DSC_PHOTO_89X119MM, MediaSizeGroup::kSizeNamed}},
      {"om_folio-sp_215x315mm",
       {PRINT_PREVIEW_MEDIA_OM_FOLIO_SP_215X315MM, MediaSizeGroup::kSizeMm}},
      {"om_folio_210x330mm",
       {PRINT_PREVIEW_MEDIA_OM_FOLIO_210X330MM, MediaSizeGroup::kSizeMm}},
      {"om_invite_220x220mm",
       {PRINT_PREVIEW_MEDIA_OM_INVITE_220X220MM, MediaSizeGroup::kSizeNamed}},
      {"om_italian_110x230mm",
       {PRINT_PREVIEW_MEDIA_OM_ITALIAN_110X230MM, MediaSizeGroup::kSizeNamed}},
      {"om_juuro-ku-kai_198x275mm",
       {PRINT_PREVIEW_MEDIA_OM_JUURO_KU_KAI_198X275MM,
        MediaSizeGroup::kSizeMm}},
      // Duplicate of the next because this was previously mapped wrong in
      // cups.
      {"om_large-photo_200x300",
       {PRINT_PREVIEW_MEDIA_OM_LARGE_PHOTO_200X300, MediaSizeGroup::kSizeMm}},
      {"om_large-photo_200x300mm",
       {PRINT_PREVIEW_MEDIA_OM_LARGE_PHOTO_200X300, MediaSizeGroup::kSizeMm}},
      {"om_medium-photo_130x180mm",
       {PRINT_PREVIEW_MEDIA_OM_MEDIUM_PHOTO_130X180MM,
        MediaSizeGroup::kSizeMm}},
      {"om_pa-kai_267x389mm",
       {PRINT_PREVIEW_MEDIA_OM_PA_KAI_267X389MM, MediaSizeGroup::kSizeMm}},
      {"om_photo-30x40_300x400mm",
       {PRINT_PREVIEW_MEDIA_OM_PHOTO_30X40_300X400MM, MediaSizeGroup::kSizeMm}},
      {"om_photo-30x45_300x450mm",
       {PRINT_PREVIEW_MEDIA_OM_PHOTO_30X45_300X450MM, MediaSizeGroup::kSizeMm}},
      {"om_photo-35x46_350x460mm",
       {PRINT_PREVIEW_MEDIA_OM_PHOTO_35X46_350X460MM, MediaSizeGroup::kSizeMm}},
      {"om_photo-40x60_400x600mm",
       {PRINT_PREVIEW_MEDIA_OM_PHOTO_40X60_400X600MM, MediaSizeGroup::kSizeMm}},
      {"om_photo-50x75_500x750mm",
       {PRINT_PREVIEW_MEDIA_OM_PHOTO_50X75_500X750MM, MediaSizeGroup::kSizeMm}},
      {"om_photo-50x76_500x760mm",
       {PRINT_PREVIEW_MEDIA_OM_PHOTO_50X76_500X760MM, MediaSizeGroup::kSizeMm}},
      {"om_photo-60x90_600x900mm",
       {PRINT_PREVIEW_MEDIA_OM_PHOTO_60X90_600X900MM, MediaSizeGroup::kSizeMm}},
      // Duplicate of iso_c6c5_114x229mm.
      {"om_postfix_114x229mm",
       {PRINT_PREVIEW_MEDIA_ISO_C6C5_114X229MM, MediaSizeGroup::kSizeNamed}},
      {"om_small-photo_100x150mm",
       {PRINT_PREVIEW_MEDIA_OM_SMALL_PHOTO_100X150MM, MediaSizeGroup::kSizeMm}},
      {"om_square-photo_89x89mm",
       {PRINT_PREVIEW_MEDIA_OM_SQUARE_PHOTO_89X89MM, MediaSizeGroup::kSizeMm}},
      {"om_wide-photo_100x200mm",
       {PRINT_PREVIEW_MEDIA_OM_WIDE_PHOTO_100X200MM, MediaSizeGroup::kSizeMm}},
      // Duplicate of iso_c3_324x458mm.
      {"prc_10_324x458mm",
       {PRINT_PREVIEW_MEDIA_ISO_C3_324X458MM, MediaSizeGroup::kSizeNamed}},
      {"prc_16k_146x215mm",
       {PRINT_PREVIEW_MEDIA_PRC_16K_146X215MM, MediaSizeGroup::kSizeNamed}},
      {"prc_1_102x165mm",
       {PRINT_PREVIEW_MEDIA_PRC_1_102X165MM, MediaSizeGroup::kSizeNamed}},
      {"prc_2_102x176mm",
       {PRINT_PREVIEW_MEDIA_PRC_2_102X176MM, MediaSizeGroup::kSizeNamed}},
      {"prc_32k_97x151mm",
       {PRINT_PREVIEW_MEDIA_PRC_32K_97X151MM, MediaSizeGroup::kSizeNamed}},
      // Duplicate of iso_b6_125x176mm.
      {"prc_3_125x176mm",
       {PRINT_PREVIEW_MEDIA_ISO_B6_125X176MM, MediaSizeGroup::kSizeNamed}},
      {"prc_4_110x208mm",
       {PRINT_PREVIEW_MEDIA_PRC_4_110X208MM, MediaSizeGroup::kSizeNamed}},
      // Duplicate of iso_dl_110x220mm.
      {"prc_5_110x220mm",
       {PRINT_PREVIEW_MEDIA_ISO_DL_110X220MM, MediaSizeGroup::kSizeNamed}},
      {"prc_6_120x320mm",
       {PRINT_PREVIEW_MEDIA_PRC_6_120X320MM, MediaSizeGroup::kSizeNamed}},
      {"prc_7_160x230mm",
       {PRINT_PREVIEW_MEDIA_PRC_7_160X230MM, MediaSizeGroup::kSizeNamed}},
      {"prc_8_120x309mm",
       {PRINT_PREVIEW_MEDIA_PRC_8_120X309MM, MediaSizeGroup::kSizeNamed}},
      {"roc_16k_7.75x10.75in",
       {PRINT_PREVIEW_MEDIA_ROC_16K_7_75X10_75IN, MediaSizeGroup::kSizeNamed}},
      {"roc_8k_10.75x15.5in",
       {PRINT_PREVIEW_MEDIA_ROC_8K_10_75X15_5IN, MediaSizeGroup::kSizeNamed}},
  });

  auto* it = kMediaMap.find(vendor_id);
  return it != kMediaMap.end()
             ? MediaSizeInfo{l10n_util::GetStringUTF16(it->second.first),
                             it->second.second, /*registered_size=*/true}
             : MediaSizeInfo{u"", MediaSizeGroup::kSizeNamed,
                             /*registered_size=*/false};
}

// Generate a human-readable name and sort group from a PWG self-describing
// name.  If `pwg_name` is not a valid self-describing media size, the returned
// name will be empty.
MediaSizeInfo InfoForSelfDescribingSize(const std::string& pwg_name) {
  // The expected format is area_description_dimensions, and dimensions are
  // WxHmm or WxHin.  Both W and H can contain decimals.
  static const base::NoDestructor<re2::RE2> media_name_pattern(
      "[^_]+_([^_]+)_([\\d.]+)x([\\d.]+)(in|mm)");
  std::string description;
  std::string width;
  std::string height;
  std::string unit_str;
  if (!RE2::FullMatch(pwg_name, *media_name_pattern, &description, &width,
                      &height, &unit_str)) {
    PRINTER_LOG(ERROR) << "Can't generate name for invalid IPP media size "
                       << pwg_name;
    return {u"", MediaSizeGroup::kSizeNamed, /*registered_size=*/false};
  }
  Unit units = unit_str == "in" ? Unit::kInches : Unit::kMillimeters;

  // If the name appears to end with approximately the paper dimensions, just
  // display the dimensions.  This avoids having things like "Card 4x6" and
  // "4 X 7" mixed with "4 x 6 in".
  static const base::NoDestructor<re2::RE2> description_dimensions_pattern(
      ".*\\b([\\d.]+)-?x-?([\\d.]+)(in|mm)?$");
  std::string name_width;
  std::string name_height;
  if (RE2::FullMatch(description, *description_dimensions_pattern, &name_width,
                     &name_height) &&
      base::StartsWith(width, name_width) &&
      base::StartsWith(height, name_height)) {
    switch (units) {
      case Unit::kInches:
        return {l10n_util::GetStringFUTF16(
                    PRINT_PREVIEW_MEDIA_DIMENSIONS_INCHES,
                    base::ASCIIToUTF16(width), base::ASCIIToUTF16(height)),
                MediaSizeGroup::kSizeIn, /*registered_size=*/false};

      case Unit::kMillimeters:
        return {l10n_util::GetStringFUTF16(PRINT_PREVIEW_MEDIA_DIMENSIONS_MM,
                                           base::ASCIIToUTF16(width),
                                           base::ASCIIToUTF16(height)),
                MediaSizeGroup::kSizeMm, /*registered_size=*/false};
    }
  }

  // For other names, attempt to generate a readable name by splitting into
  // words and title-casing each word.  Self-describing names are always ASCII,
  // so it is safe to do case conversion without considering locales.  We don't
  // have any way to know if the results unambiguously describe a paper size the
  // user would recognize, so also append the dimensions.  The final output is
  // dependent on the quality of the descriptions provided by the printer, but
  // should in any case be better than simply displaying the raw region and
  // description.
  std::vector<std::string> words = base::SplitString(
      description, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (std::string& word : words) {
    word[0] = base::ToUpperASCII(word[0]);  // Safe due to NONEMPTY split above.
    for (size_t i = 1; i < word.size(); i++) {
      word[i] = base::ToLowerASCII(word[i]);
    }
  }
  std::string clean_name = base::JoinString(words, " ");

  switch (units) {
    case Unit::kInches:
      return {l10n_util::GetStringFUTF16(
                  PRINT_PREVIEW_MEDIA_NAME_WITH_DIMENSIONS_INCHES,
                  base::ASCIIToUTF16(clean_name), base::ASCIIToUTF16(width),
                  base::ASCIIToUTF16(height)),
              MediaSizeGroup::kSizeNamed, /*registered_size=*/false};

    case Unit::kMillimeters:
      return {l10n_util::GetStringFUTF16(
                  PRINT_PREVIEW_MEDIA_NAME_WITH_DIMENSIONS_MM,
                  base::ASCIIToUTF16(clean_name), base::ASCIIToUTF16(width),
                  base::ASCIIToUTF16(height)),
              MediaSizeGroup::kSizeNamed, /*registered_size=*/false};
  }
}

}  // namespace

PaperWithSizeInfo::PaperWithSizeInfo(MediaSizeInfo msi,
                                     PrinterSemanticCapsAndDefaults::Paper p)
    : size_info(msi), paper(p) {}

MediaSizeInfo LocalizePaperDisplayName(const std::string& vendor_id) {
  // We can't do anything without a vendor ID.
  if (vendor_id.empty()) {
    return {u"", MediaSizeGroup::kSizeNamed, /*registered_size=*/false};
  }

  MediaSizeInfo size_info = InfoForVendorId(vendor_id);
  if (size_info.name.empty()) {
    // If it wasn't a standard PWG media size name, check to see if there
    // is a standard name with the same dimensions.
    std::string std_vendor_id = std::string(StandardNameForSize(vendor_id));
    if (!std_vendor_id.empty()) {
      PRINTER_LOG(DEBUG) << "Mapped non-standard media name " << vendor_id
                         << " to " << std_vendor_id;
      size_info = InfoForVendorId(std_vendor_id);
      size_info.registered_size = false;
    }
  }
  return size_info.name.empty() ? InfoForSelfDescribingSize(vendor_id)
                                : size_info;
}

void SortPaperDisplayNames(std::vector<PaperWithSizeInfo>& papers) {
  std::vector<PaperWithSizeInfo> mm_sizes;
  std::vector<PaperWithSizeInfo> in_sizes;
  std::vector<PaperWithSizeInfo> named_sizes;

  // Break apart the list into separate sort groups.
  for (auto& p : papers) {
    // Drop borderless sizes so they don't get sorted ahead of standard sizes.
    // TODO(b/218752273): Remove once borderless sizes are handled properly.
    if (base::Contains(p.paper.vendor_id, ".borderless_") ||
        base::Contains(p.paper.vendor_id, ".fb_")) {
      continue;
    }

    switch (p.size_info.sort_group) {
      case MediaSizeGroup::kSizeMm:
        mm_sizes.emplace_back(p);
        break;

      case MediaSizeGroup::kSizeIn:
        in_sizes.emplace_back(p);
        break;

      case MediaSizeGroup::kSizeNamed:
        named_sizes.emplace_back(p);
        break;
    }
  }

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(icu::Collator::createInstance(error));
  DCHECK(U_SUCCESS(error));

  // Sort dimensional sizes (inch and mm) by width, then height, then name.
  // Official sizes come before unregistered sizes if they have the same
  // dimensions.
  auto size_sort = [&collator](const PaperWithSizeInfo& a,
                               const PaperWithSizeInfo& b) {
    const gfx::Size& size_a = a.paper.size_um;
    const gfx::Size& size_b = b.paper.size_um;

    if (size_a.width() != size_b.width())
      return size_a.width() < size_b.width();

    if (size_a.height() != size_b.height())
      return size_a.height() < size_b.height();

    if (a.size_info.registered_size != b.size_info.registered_size) {
      return a.size_info.registered_size;
    }

    // Same dimensions and official status.  Sort by display name.
    UCollationResult comp = base::i18n::CompareString16WithCollator(
        *collator, a.size_info.name, b.size_info.name);
    return comp == UCOL_LESS;
  };
  std::sort(mm_sizes.begin(), mm_sizes.end(), size_sort);
  std::sort(in_sizes.begin(), in_sizes.end(), size_sort);

  // Sort named sizes by name, then width, then height.  Official sizes with the
  // same name come before unofficial sizes.
  auto name_sort = [&collator](const PaperWithSizeInfo& a,
                               const PaperWithSizeInfo& b) {
    const gfx::Size& size_a = a.paper.size_um;
    const gfx::Size& size_b = b.paper.size_um;

    UCollationResult comp = base::i18n::CompareString16WithCollator(
        *collator, a.size_info.name, b.size_info.name);
    if (comp != UCOL_EQUAL)
      return comp == UCOL_LESS;

    // Same name.  Sort registered sizes ahead of unofficial sizes.
    if (a.size_info.registered_size != b.size_info.registered_size) {
      return a.size_info.registered_size;
    }

    // Same name and registration status.  Sort by width, then height.
    if (size_a.width() != size_b.width())
      return size_a.width() < size_b.width();
    return size_a.height() < size_b.height();
  };
  std::sort(named_sizes.begin(), named_sizes.end(), name_sort);

  // Replace the original list with the newly sorted groups.
  papers.clear();
  papers.insert(papers.end(), in_sizes.begin(), in_sizes.end());
  papers.insert(papers.end(), mm_sizes.begin(), mm_sizes.end());
  papers.insert(papers.end(), named_sizes.begin(), named_sizes.end());
}

}  // namespace printing
