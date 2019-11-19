// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_media_l10n.h"

#include <map>

#include "base/no_destructor.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace printing {

namespace {

// Return the resource ID of a media name specified by |vendor_id|
// if any is found - else return -1. The static map contained here
// is intended to reach all translated media names - see
// print_media_resources.grd.
int VendorIdToTranslatedId(const std::string& vendor_id) {
  static const base::NoDestructor<std::map<std::string, int>> media_map({
      {"asme_f_28x40in", PRINT_PREVIEW_MEDIA_ASME_F_28X40IN},
      {"iso_2a0_1189x1682mm", PRINT_PREVIEW_MEDIA_ISO_2A0_1189X1682MM},
      {"iso_a0_841x1189mm", PRINT_PREVIEW_MEDIA_ISO_A0_841X1189MM},
      {"iso_a10_26x37mm", PRINT_PREVIEW_MEDIA_ISO_A10_26X37MM},
      {"iso_a1_594x841mm", PRINT_PREVIEW_MEDIA_ISO_A1_594X841MM},
      {"iso_a2_420x594mm", PRINT_PREVIEW_MEDIA_ISO_A2_420X594MM},
      {"iso_a3_297x420mm", PRINT_PREVIEW_MEDIA_ISO_A3_297X420MM},
      {"iso_a4-extra_235.5x322.3mm",
       PRINT_PREVIEW_MEDIA_ISO_A4_EXTRA_235_5X322_3MM},
      {"iso_a4-tab_225x297mm", PRINT_PREVIEW_MEDIA_ISO_A4_TAB_225X297MM},
      {"iso_a4_210x297mm", PRINT_PREVIEW_MEDIA_ISO_A4_210X297MM},
      {"iso_a5-extra_174x235mm", PRINT_PREVIEW_MEDIA_ISO_A5_EXTRA_174X235MM},
      {"iso_a5_148x210mm", PRINT_PREVIEW_MEDIA_ISO_A5_148X210MM},
      {"iso_a6_105x148mm", PRINT_PREVIEW_MEDIA_ISO_A6_105X148MM},
      {"iso_a7_74x105mm", PRINT_PREVIEW_MEDIA_ISO_A7_74X105MM},
      {"iso_a8_52x74mm", PRINT_PREVIEW_MEDIA_ISO_A8_52X74MM},
      {"iso_a9_37x52mm", PRINT_PREVIEW_MEDIA_ISO_A9_37X52MM},
      {"iso_b0_1000x1414mm", PRINT_PREVIEW_MEDIA_ISO_B0_1000X1414MM},
      {"iso_b10_31x44mm", PRINT_PREVIEW_MEDIA_ISO_B10_31X44MM},
      {"iso_b1_707x1000mm", PRINT_PREVIEW_MEDIA_ISO_B1_707X1000MM},
      {"iso_b2_500x707mm", PRINT_PREVIEW_MEDIA_ISO_B2_500X707MM},
      {"iso_b3_353x500mm", PRINT_PREVIEW_MEDIA_ISO_B3_353X500MM},
      {"iso_b4_250x353mm", PRINT_PREVIEW_MEDIA_ISO_B4_250X353MM},
      {"iso_b5-extra_201x276mm", PRINT_PREVIEW_MEDIA_ISO_B5_EXTRA_201X276MM},
      {"iso_b5_176x250mm", PRINT_PREVIEW_MEDIA_ISO_B5_176X250MM},
      {"iso_b6_125x176mm", PRINT_PREVIEW_MEDIA_ISO_B6_125X176MM},
      {"iso_b6c4_125x324mm", PRINT_PREVIEW_MEDIA_ISO_B6C4_125X324MM},
      {"iso_b7_88x125mm", PRINT_PREVIEW_MEDIA_ISO_B7_88X125MM},
      {"iso_b8_62x88mm", PRINT_PREVIEW_MEDIA_ISO_B8_62X88MM},
      {"iso_b9_44x62mm", PRINT_PREVIEW_MEDIA_ISO_B9_44X62MM},
      {"iso_c0_917x1297mm", PRINT_PREVIEW_MEDIA_ISO_C0_917X1297MM},
      {"iso_c10_28x40mm", PRINT_PREVIEW_MEDIA_ISO_C10_28X40MM},
      {"iso_c1_648x917mm", PRINT_PREVIEW_MEDIA_ISO_C1_648X917MM},
      {"iso_c2_458x648mm", PRINT_PREVIEW_MEDIA_ISO_C2_458X648MM},
      {"iso_c3_324x458mm", PRINT_PREVIEW_MEDIA_ISO_C3_324X458MM},
      {"iso_c4_229x324mm", PRINT_PREVIEW_MEDIA_ISO_C4_229X324MM},
      {"iso_c5_162x229mm", PRINT_PREVIEW_MEDIA_ISO_C5_162X229MM},
      {"iso_c6_114x162mm", PRINT_PREVIEW_MEDIA_ISO_C6_114X162MM},
      {"iso_c6c5_114x229mm", PRINT_PREVIEW_MEDIA_ISO_C6C5_114X229MM},
      {"iso_c7_81x114mm", PRINT_PREVIEW_MEDIA_ISO_C7_81X114MM},
      {"iso_c7c6_81x162mm", PRINT_PREVIEW_MEDIA_ISO_C7C6_81X162MM},
      {"iso_c8_57x81mm", PRINT_PREVIEW_MEDIA_ISO_C8_57X81MM},
      {"iso_c9_40x57mm", PRINT_PREVIEW_MEDIA_ISO_C9_40X57MM},
      {"iso_dl_110x220mm", PRINT_PREVIEW_MEDIA_ISO_DL_110X220MM},
      {"jis_exec_216x330mm", PRINT_PREVIEW_MEDIA_JIS_EXEC_216X330MM},
      {"jpn_chou2_111.1x146mm", PRINT_PREVIEW_MEDIA_JPN_CHOU2_111_1X146MM},
      {"jpn_chou3_120x235mm", PRINT_PREVIEW_MEDIA_JPN_CHOU3_120X235MM},
      {"jpn_chou4_90x205mm", PRINT_PREVIEW_MEDIA_JPN_CHOU4_90X205MM},
      {"jpn_hagaki_100x148mm", PRINT_PREVIEW_MEDIA_JPN_HAGAKI_100X148MM},
      {"jpn_kahu_240x322.1mm", PRINT_PREVIEW_MEDIA_JPN_KAHU_240X322_1MM},
      {"jpn_kaku2_240x332mm", PRINT_PREVIEW_MEDIA_JPN_KAKU2_240X332MM},
      {"jpn_oufuku_148x200mm", PRINT_PREVIEW_MEDIA_JPN_OUFUKU_148X200MM},
      {"jpn_you4_105x235mm", PRINT_PREVIEW_MEDIA_JPN_YOU4_105X235MM},
      {"na_10x11_10x11in", PRINT_PREVIEW_MEDIA_NA_10X11_10X11IN},
      {"na_10x13_10x13in", PRINT_PREVIEW_MEDIA_NA_10X13_10X13IN},
      {"na_10x14_10x14in", PRINT_PREVIEW_MEDIA_NA_10X14_10X14IN},
      {"na_10x15_10x15in", PRINT_PREVIEW_MEDIA_NA_10X15_10X15IN},
      {"na_11x12_11x12in", PRINT_PREVIEW_MEDIA_NA_11X12_11X12IN},
      {"na_11x15_11x15in", PRINT_PREVIEW_MEDIA_NA_11X15_11X15IN},
      {"na_12x19_12x19in", PRINT_PREVIEW_MEDIA_NA_12X19_12X19IN},
      {"na_5x7_5x7in", PRINT_PREVIEW_MEDIA_NA_5X7_5X7IN},
      {"na_6x9_6x9in", PRINT_PREVIEW_MEDIA_NA_6X9_6X9IN},
      {"na_7x9_7x9in", PRINT_PREVIEW_MEDIA_NA_7X9_7X9IN},
      {"na_9x11_9x11in", PRINT_PREVIEW_MEDIA_NA_9X11_9X11IN},
      {"na_a2_4.375x5.75in", PRINT_PREVIEW_MEDIA_NA_A2_4_375X5_75IN},
      {"na_arch-a_9x12in", PRINT_PREVIEW_MEDIA_NA_ARCH_A_9X12IN},
      {"na_arch-b_12x18in", PRINT_PREVIEW_MEDIA_NA_ARCH_B_12X18IN},
      {"na_arch-c_18x24in", PRINT_PREVIEW_MEDIA_NA_ARCH_C_18X24IN},
      {"na_arch-d_24x36in", PRINT_PREVIEW_MEDIA_NA_ARCH_D_24X36IN},
      {"na_arch-e_36x48in", PRINT_PREVIEW_MEDIA_NA_ARCH_E_36X48IN},
      {"na_b-plus_12x19.17in", PRINT_PREVIEW_MEDIA_NA_B_PLUS_12X19_17IN},
      {"na_c5_6.5x9.5in", PRINT_PREVIEW_MEDIA_NA_C5_6_5X9_5IN},
      {"na_c_17x22in", PRINT_PREVIEW_MEDIA_NA_C_17X22IN},
      {"na_d_22x34in", PRINT_PREVIEW_MEDIA_NA_D_22X34IN},
      {"na_e_34x44in", PRINT_PREVIEW_MEDIA_NA_E_34X44IN},
      {"na_edp_11x14in", PRINT_PREVIEW_MEDIA_NA_EDP_11X14IN},
      {"na_eur-edp_12x14in", PRINT_PREVIEW_MEDIA_NA_EUR_EDP_12X14IN},
      {"na_f_44x68in", PRINT_PREVIEW_MEDIA_NA_F_44X68IN},
      {"na_fanfold-eur_8.5x12in", PRINT_PREVIEW_MEDIA_NA_FANFOLD_EUR_8_5X12IN},
      {"na_fanfold-us_11x14.875in",
       PRINT_PREVIEW_MEDIA_NA_FANFOLD_US_11X14_875IN},
      {"na_foolscap_8.5x13in", PRINT_PREVIEW_MEDIA_NA_FOOLSCAP_8_5X13IN},
      {"na_govt-legal_8x13in", PRINT_PREVIEW_MEDIA_NA_GOVT_LEGAL_8X13IN},
      {"na_govt-letter_8x10in", PRINT_PREVIEW_MEDIA_NA_GOVT_LETTER_8X10IN},
      {"na_index-3x5_3x5in", PRINT_PREVIEW_MEDIA_NA_INDEX_3X5_3X5IN},
      {"na_index-4x6-ext_6x8in", PRINT_PREVIEW_MEDIA_NA_INDEX_4X6_EXT_6X8IN},
      {"na_index-4x6_4x6in", PRINT_PREVIEW_MEDIA_NA_INDEX_4X6_4X6IN},
      {"na_index-5x8_5x8in", PRINT_PREVIEW_MEDIA_NA_INDEX_5X8_5X8IN},
      {"na_invoice_5.5x8.5in", PRINT_PREVIEW_MEDIA_NA_INVOICE_5_5X8_5IN},
      {"na_ledger_11x17in", PRINT_PREVIEW_MEDIA_NA_LEDGER_11X17IN},
      {"na_legal-extra_9.5x15in", PRINT_PREVIEW_MEDIA_NA_LEGAL_EXTRA_9_5X15IN},
      {"na_legal_8.5x14in", PRINT_PREVIEW_MEDIA_NA_LEGAL_8_5X14IN},
      {"na_letter-extra_9.5x12in",
       PRINT_PREVIEW_MEDIA_NA_LETTER_EXTRA_9_5X12IN},
      {"na_letter-plus_8.5x12.69in",
       PRINT_PREVIEW_MEDIA_NA_LETTER_PLUS_8_5X12_69IN},
      {"na_letter_8.5x11in", PRINT_PREVIEW_MEDIA_NA_LETTER_8_5X11IN},
      {"na_number-10_4.125x9.5in",
       PRINT_PREVIEW_MEDIA_NA_NUMBER_10_4_125X9_5IN},
      {"na_number-11_4.5x10.375in",
       PRINT_PREVIEW_MEDIA_NA_NUMBER_11_4_5X10_375IN},
      {"na_number-12_4.75x11in", PRINT_PREVIEW_MEDIA_NA_NUMBER_12_4_75X11IN},
      {"na_number-14_5x11.5in", PRINT_PREVIEW_MEDIA_NA_NUMBER_14_5X11_5IN},
      {"na_personal_3.625x6.5in", PRINT_PREVIEW_MEDIA_NA_PERSONAL_3_625X6_5IN},
      {"na_super-a_8.94x14in", PRINT_PREVIEW_MEDIA_NA_SUPER_A_8_94X14IN},
      {"na_super-b_13x19in", PRINT_PREVIEW_MEDIA_NA_SUPER_B_13X19IN},
      {"na_wide-format_30x42in", PRINT_PREVIEW_MEDIA_NA_WIDE_FORMAT_30X42IN},
      {"om_dai-pa-kai_275x395mm", PRINT_PREVIEW_MEDIA_OM_DAI_PA_KAI_275X395MM},
      {"om_folio-sp_215x315mm", PRINT_PREVIEW_MEDIA_OM_FOLIO_SP_215X315MM},
      {"om_invite_220x220mm", PRINT_PREVIEW_MEDIA_OM_INVITE_220X220MM},
      {"om_italian_110x230mm", PRINT_PREVIEW_MEDIA_OM_ITALIAN_110X230MM},
      {"om_juuro-ku-kai_198x275mm",
       PRINT_PREVIEW_MEDIA_OM_JUURO_KU_KAI_198X275MM},
      {"om_large-photo_200x300", PRINT_PREVIEW_MEDIA_OM_LARGE_PHOTO_200X300},
      {"om_pa-kai_267x389mm", PRINT_PREVIEW_MEDIA_OM_PA_KAI_267X389MM},
      {"om_postfix_114x229mm", PRINT_PREVIEW_MEDIA_OM_POSTFIX_114X229MM},
      {"om_small-photo_100x150mm",
       PRINT_PREVIEW_MEDIA_OM_SMALL_PHOTO_100X150MM},
      {"prc_10_324x458mm", PRINT_PREVIEW_MEDIA_PRC_10_324X458MM},
      {"prc_16k_146x215mm", PRINT_PREVIEW_MEDIA_PRC_16K_146X215MM},
      {"prc_1_102x165mm", PRINT_PREVIEW_MEDIA_PRC_1_102X165MM},
      {"prc_2_102x176mm", PRINT_PREVIEW_MEDIA_PRC_2_102X176MM},
      {"prc_32k_97x151mm", PRINT_PREVIEW_MEDIA_PRC_32K_97X151MM},
      {"prc_3_125x176mm", PRINT_PREVIEW_MEDIA_PRC_3_125X176MM},
      {"prc_4_110x208mm", PRINT_PREVIEW_MEDIA_PRC_4_110X208MM},
      {"prc_5_110x220mm", PRINT_PREVIEW_MEDIA_PRC_5_110X220MM},
      {"prc_6_120x320mm", PRINT_PREVIEW_MEDIA_PRC_6_120X320MM},
      {"prc_7_160x230mm", PRINT_PREVIEW_MEDIA_PRC_7_160X230MM},
      {"prc_8_120x309mm", PRINT_PREVIEW_MEDIA_PRC_8_120X309MM},
      {"roc_16k_7.75x10.75in", PRINT_PREVIEW_MEDIA_ROC_16K_7_75X10_75IN},
      {"roc_8k_10.75x15.5in", PRINT_PREVIEW_MEDIA_ROC_8K_10_75X15_5IN},

      // Here follow manually curated IDs not blessed with common names
      // in PWG 5101.1-2002.

      // JIS B*
      {"jis_b0_1030x1456mm", PRINT_PREVIEW_MEDIA_JIS_B0_1030X1456MM},
      {"jis_b1_728x1030mm", PRINT_PREVIEW_MEDIA_JIS_B1_728X1030MM},
      {"jis_b2_515x728mm", PRINT_PREVIEW_MEDIA_JIS_B2_515X728MM},
      {"jis_b3_364x515mm", PRINT_PREVIEW_MEDIA_JIS_B3_364X515MM},
      {"jis_b4_257x364mm", PRINT_PREVIEW_MEDIA_JIS_B4_257X364MM},
      {"jis_b5_182x257mm", PRINT_PREVIEW_MEDIA_JIS_B5_182X257MM},
      {"jis_b6_128x182mm", PRINT_PREVIEW_MEDIA_JIS_B6_128X182MM},
      {"jis_b7_91x128mm", PRINT_PREVIEW_MEDIA_JIS_B7_91X128MM},
      {"jis_b8_64x91mm", PRINT_PREVIEW_MEDIA_JIS_B8_64X91MM},
      {"jis_b9_45x64mm", PRINT_PREVIEW_MEDIA_JIS_B9_45X64MM},
      {"jis_b10_32x45mm", PRINT_PREVIEW_MEDIA_JIS_B10_32X45MM},
  });

  auto it = media_map->find(vendor_id);
  return it != media_map->end() ? it->second : -1;
}

}  // namespace

std::string LocalizePaperDisplayName(const std::string& vendor_id) {
  // We can't do anything without a vendor ID.
  if (vendor_id.empty()) {
    return std::string();
  }

  int translation_id = VendorIdToTranslatedId(vendor_id);
  return translation_id < 0 ? std::string()
                            : l10n_util::GetStringUTF8(translation_id);
}

}  // namespace printing
