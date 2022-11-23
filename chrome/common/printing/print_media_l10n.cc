// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/printing/print_media_l10n.h"

#include <map>

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

// Return the resource ID of a media name specified by |vendor_id|
// if any is found - else return -1. The static map contained here
// is intended to reach all translated media names - see
// print_media_resources.grd.
int VendorIdToTranslatedId(const std::string& vendor_id) {
  static const base::NoDestructor<std::map<std::string, int>> media_map({
      {"asme_f_28x40in", PRINT_PREVIEW_MEDIA_ASME_F_28X40IN},
      {"iso_2a0_1189x1682mm", PRINT_PREVIEW_MEDIA_ISO_2A0_1189X1682MM},
      {"iso_a0_841x1189mm", PRINT_PREVIEW_MEDIA_ISO_A0_841X1189MM},
      {"iso_a0x3_1189x2523mm", PRINT_PREVIEW_MEDIA_ISO_A0X3_1189X2523MM},
      {"iso_a10_26x37mm", PRINT_PREVIEW_MEDIA_ISO_A10_26X37MM},
      {"iso_a1_594x841mm", PRINT_PREVIEW_MEDIA_ISO_A1_594X841MM},
      {"iso_a1x3_841x1783mm", PRINT_PREVIEW_MEDIA_ISO_A1X3_841X1783MM},
      {"iso_a1x4_841x2378mm", PRINT_PREVIEW_MEDIA_ISO_A1X4_841X2378MM},
      {"iso_a2_420x594mm", PRINT_PREVIEW_MEDIA_ISO_A2_420X594MM},
      {"iso_a2x3_594x1261mm", PRINT_PREVIEW_MEDIA_ISO_A2X3_594X1261MM},
      {"iso_a2x4_594x1682mm", PRINT_PREVIEW_MEDIA_ISO_A2X4_594X1682MM},
      {"iso_a2x5_594x2102mm", PRINT_PREVIEW_MEDIA_ISO_A2X5_594X2102MM},
      {"iso_a3-extra_322x445mm", PRINT_PREVIEW_MEDIA_ISO_A3_EXTRA_322X445MM},
      {"iso_a3_297x420mm", PRINT_PREVIEW_MEDIA_ISO_A3_297X420MM},
      {"iso_a3x3_420x891mm", PRINT_PREVIEW_MEDIA_ISO_A3X3_420X891MM},
      {"iso_a3x4_420x1189mm", PRINT_PREVIEW_MEDIA_ISO_A3X4_420X1189MM},
      {"iso_a3x5_420x1486mm", PRINT_PREVIEW_MEDIA_ISO_A3X5_420X1486MM},
      {"iso_a3x6_420x1783mm", PRINT_PREVIEW_MEDIA_ISO_A3X6_420X1783MM},
      {"iso_a3x7_420x2080mm", PRINT_PREVIEW_MEDIA_ISO_A3X7_420X2080MM},
      {"iso_a4-extra_235.5x322.3mm",
       PRINT_PREVIEW_MEDIA_ISO_A4_EXTRA_235_5X322_3MM},
      {"iso_a4-tab_225x297mm", PRINT_PREVIEW_MEDIA_ISO_A4_TAB_225X297MM},
      {"iso_a4_210x297mm", PRINT_PREVIEW_MEDIA_ISO_A4_210X297MM},
      {"iso_a4x3_297x630mm", PRINT_PREVIEW_MEDIA_ISO_A4X3_297X630MM},
      {"iso_a4x4_297x841mm", PRINT_PREVIEW_MEDIA_ISO_A4X4_297X841MM},
      {"iso_a4x5_297x1051mm", PRINT_PREVIEW_MEDIA_ISO_A4X5_297X1051MM},
      {"iso_a4x6_297x1261mm", PRINT_PREVIEW_MEDIA_ISO_A4X6_297X1261MM},
      {"iso_a4x7_297x1471mm", PRINT_PREVIEW_MEDIA_ISO_A4X7_297X1471MM},
      {"iso_a4x8_297x1682mm", PRINT_PREVIEW_MEDIA_ISO_A4X8_297X1682MM},
      {"iso_a4x9_297x1892mm", PRINT_PREVIEW_MEDIA_ISO_A4X9_297X1892MM},
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
      {"iso_id-1_53.98x85.6mm", PRINT_PREVIEW_MEDIA_ISO_ID_1_53_98X85_6MM},
      // Duplicate of iso_b7_88x125mm.
      {"iso_id-3_88x125mm", PRINT_PREVIEW_MEDIA_ISO_B7_88X125MM},
      {"iso_ra0_860x1220mm", PRINT_PREVIEW_MEDIA_ISO_RA0_860X1220MM},
      {"iso_ra1_610x860mm", PRINT_PREVIEW_MEDIA_ISO_RA1_610X860MM},
      {"iso_ra2_430x610mm", PRINT_PREVIEW_MEDIA_ISO_RA2_430X610MM},
      {"iso_ra3_305x430mm", PRINT_PREVIEW_MEDIA_ISO_RA3_305X430MM},
      {"iso_ra4_215x305mm", PRINT_PREVIEW_MEDIA_ISO_RA4_215X305MM},
      {"iso_sra0_900x1280mm", PRINT_PREVIEW_MEDIA_ISO_SRA0_900X1280MM},
      {"iso_sra1_640x900mm", PRINT_PREVIEW_MEDIA_ISO_SRA1_640X900MM},
      {"iso_sra2_450x640mm", PRINT_PREVIEW_MEDIA_ISO_SRA2_450X640MM},
      {"iso_sra3_320x450mm", PRINT_PREVIEW_MEDIA_ISO_SRA3_320X450MM},
      {"iso_sra4_225x320mm", PRINT_PREVIEW_MEDIA_ISO_SRA4_225X320MM},
      {"jis_exec_216x330mm", PRINT_PREVIEW_MEDIA_JIS_EXEC_216X330MM},
      {"jpn_chou2_111.1x146mm", PRINT_PREVIEW_MEDIA_JPN_CHOU2_111_1X146MM},
      {"jpn_chou3_120x235mm", PRINT_PREVIEW_MEDIA_JPN_CHOU3_120X235MM},
      {"jpn_chou4_90x205mm", PRINT_PREVIEW_MEDIA_JPN_CHOU4_90X205MM},
      {"jpn_chou40_90x225mm", PRINT_PREVIEW_MEDIA_JPN_CHOU40_90X225MM},
      {"jpn_hagaki_100x148mm", PRINT_PREVIEW_MEDIA_JPN_HAGAKI_100X148MM},
      {"jpn_kahu_240x322.1mm", PRINT_PREVIEW_MEDIA_JPN_KAHU_240X322_1MM},
      {"jpn_kaku1_270x382mm", PRINT_PREVIEW_MEDIA_JPN_KAKU1_270X382MM},
      {"jpn_kaku2_240x332mm", PRINT_PREVIEW_MEDIA_JPN_KAKU2_240X332MM},
      {"jpn_kaku3_216x277mm", PRINT_PREVIEW_MEDIA_JPN_KAKU3_216X277MM},
      {"jpn_kaku4_197x267mm", PRINT_PREVIEW_MEDIA_JPN_KAKU4_197X267MM},
      {"jpn_kaku5_190x240mm", PRINT_PREVIEW_MEDIA_JPN_KAKU5_190X240MM},
      {"jpn_kaku7_142x205mm", PRINT_PREVIEW_MEDIA_JPN_KAKU7_142X205MM},
      {"jpn_kaku8_119x197mm", PRINT_PREVIEW_MEDIA_JPN_KAKU8_119X197MM},
      {"jpn_oufuku_148x200mm", PRINT_PREVIEW_MEDIA_JPN_OUFUKU_148X200MM},
      {"jpn_you4_105x235mm", PRINT_PREVIEW_MEDIA_JPN_YOU4_105X235MM},
      {"jpn_you6_98x190mm", PRINT_PREVIEW_MEDIA_JPN_YOU6_98X190MM},
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
      {"na_arch-e2_26x38in", PRINT_PREVIEW_MEDIA_NA_ARCH_E2_26X38IN},
      {"na_arch-e3_27x39in", PRINT_PREVIEW_MEDIA_NA_ARCH_E3_27X39IN},
      {"na_b-plus_12x19.17in", PRINT_PREVIEW_MEDIA_NA_B_PLUS_12X19_17IN},
      {"na_c5_6.5x9.5in", PRINT_PREVIEW_MEDIA_NA_C5_6_5X9_5IN},
      {"na_c_17x22in", PRINT_PREVIEW_MEDIA_NA_C_17X22IN},
      {"na_d_22x34in", PRINT_PREVIEW_MEDIA_NA_D_22X34IN},
      {"na_e_34x44in", PRINT_PREVIEW_MEDIA_NA_E_34X44IN},
      {"na_edp_11x14in", PRINT_PREVIEW_MEDIA_NA_EDP_11X14IN},
      {"na_eur-edp_12x14in", PRINT_PREVIEW_MEDIA_NA_EUR_EDP_12X14IN},
      {"na_executive_7.25x10.5in",
       PRINT_PREVIEW_MEDIA_NA_EXECUTIVE_7_25X10_5IN},
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
      {"na_monarch_3.875x7.5in", PRINT_PREVIEW_MEDIA_NA_MONARCH_3_875X7_5IN},
      {"na_number-9_3.875x8.875in",
       PRINT_PREVIEW_MEDIA_NA_NUMBER_9_3_875X8_875IN},
      {"na_number-10_4.125x9.5in",
       PRINT_PREVIEW_MEDIA_NA_NUMBER_10_4_125X9_5IN},
      {"na_number-11_4.5x10.375in",
       PRINT_PREVIEW_MEDIA_NA_NUMBER_11_4_5X10_375IN},
      {"na_number-12_4.75x11in", PRINT_PREVIEW_MEDIA_NA_NUMBER_12_4_75X11IN},
      {"na_number-14_5x11.5in", PRINT_PREVIEW_MEDIA_NA_NUMBER_14_5X11_5IN},
      {"na_oficio_8.5x13.4in", PRINT_PREVIEW_MEDIA_NA_OFICIO_8_5X13_4IN},
      {"na_personal_3.625x6.5in", PRINT_PREVIEW_MEDIA_NA_PERSONAL_3_625X6_5IN},
      {"na_quarto_8.5x10.83in", PRINT_PREVIEW_MEDIA_NA_QUARTO_8_5X10_83IN},
      {"na_super-a_8.94x14in", PRINT_PREVIEW_MEDIA_NA_SUPER_A_8_94X14IN},
      {"na_super-b_13x19in", PRINT_PREVIEW_MEDIA_NA_SUPER_B_13X19IN},
      {"na_wide-format_30x42in", PRINT_PREVIEW_MEDIA_NA_WIDE_FORMAT_30X42IN},
      {"oe_12x16_12x16in", PRINT_PREVIEW_MEDIA_OE_12X16_12X16IN},
      {"oe_14x17_14x17in", PRINT_PREVIEW_MEDIA_OE_14X17_14X17IN},
      {"oe_18x22_18x22in", PRINT_PREVIEW_MEDIA_OE_18X22_18X22IN},
      {"oe_a2plus_17x24in", PRINT_PREVIEW_MEDIA_OE_A2PLUS_17X24IN},
      {"oe_business-card_2x3.5in",
       PRINT_PREVIEW_MEDIA_OE_BUSINESS_CARD_2X3_5IN},
      {"oe_photo-10r_10x12in", PRINT_PREVIEW_MEDIA_OE_PHOTO_10R_10X12IN},
      {"oe_photo-12r_12x15in", PRINT_PREVIEW_MEDIA_OE_PHOTO_12R_12X15IN},
      {"oe_photo-14x18_14x18in", PRINT_PREVIEW_MEDIA_OE_PHOTO_14X18_14X18IN},
      {"oe_photo-16r_16x20in", PRINT_PREVIEW_MEDIA_OE_PHOTO_16R_16X20IN},
      {"oe_photo-20r_20x24in", PRINT_PREVIEW_MEDIA_OE_PHOTO_20R_20X24IN},
      {"oe_photo-22r_22x29.5in", PRINT_PREVIEW_MEDIA_OE_PHOTO_22R_22X29_5IN},
      {"oe_photo-22x28_22x28in", PRINT_PREVIEW_MEDIA_OE_PHOTO_22X28_22X28IN},
      {"oe_photo-24r_24x31.5in", PRINT_PREVIEW_MEDIA_OE_PHOTO_24R_24X31_5IN},
      {"oe_photo-24x30_24x30in", PRINT_PREVIEW_MEDIA_OE_PHOTO_24X30_24X30IN},
      {"oe_photo-l_3.5x5in", PRINT_PREVIEW_MEDIA_OE_PHOTO_L_3_5X5IN},
      {"oe_photo-30r_30x40in", PRINT_PREVIEW_MEDIA_OE_PHOTO_30R_30X40IN},
      {"oe_photo-s8r_8x12in", PRINT_PREVIEW_MEDIA_OE_PHOTO_S8R_8X12IN},
      // Duplicate of na_10x15_10x15in.
      {"oe_photo-s10r_10x15in", PRINT_PREVIEW_MEDIA_NA_10X15_10X15IN},
      {"oe_square-photo_4x4in", PRINT_PREVIEW_MEDIA_OE_SQUARE_PHOTO_4X4IN},
      {"oe_square-photo_5x5in", PRINT_PREVIEW_MEDIA_OE_SQUARE_PHOTO_5X5IN},
      {"om_16k_184x260mm", PRINT_PREVIEW_MEDIA_OM_16K_184X260MM},
      {"om_16k_195x270mm", PRINT_PREVIEW_MEDIA_OM_16K_195X270MM},
      {"om_business-card_55x85mm",
       PRINT_PREVIEW_MEDIA_OM_BUSINESS_CARD_55X85MM},
      {"om_business-card_55x91mm",
       PRINT_PREVIEW_MEDIA_OM_BUSINESS_CARD_55X91MM},
      {"om_card_54x86mm", PRINT_PREVIEW_MEDIA_OM_CARD_54X86MM},
      {"om_dai-pa-kai_275x395mm", PRINT_PREVIEW_MEDIA_OM_DAI_PA_KAI_275X395MM},
      {"om_dsc-photo_89x119mm", PRINT_PREVIEW_MEDIA_OM_DSC_PHOTO_89X119MM},
      {"om_folio-sp_215x315mm", PRINT_PREVIEW_MEDIA_OM_FOLIO_SP_215X315MM},
      {"om_folio_210x330mm", PRINT_PREVIEW_MEDIA_OM_FOLIO_210X330MM},
      {"om_invite_220x220mm", PRINT_PREVIEW_MEDIA_OM_INVITE_220X220MM},
      {"om_italian_110x230mm", PRINT_PREVIEW_MEDIA_OM_ITALIAN_110X230MM},
      {"om_juuro-ku-kai_198x275mm",
       PRINT_PREVIEW_MEDIA_OM_JUURO_KU_KAI_198X275MM},
      // Duplicate of the next because this was previously mapped wrong in cups.
      {"om_large-photo_200x300", PRINT_PREVIEW_MEDIA_OM_LARGE_PHOTO_200X300},
      {"om_large-photo_200x300mm", PRINT_PREVIEW_MEDIA_OM_LARGE_PHOTO_200X300},
      {"om_medium-photo_130x180mm",
       PRINT_PREVIEW_MEDIA_OM_MEDIUM_PHOTO_130X180MM},
      {"om_pa-kai_267x389mm", PRINT_PREVIEW_MEDIA_OM_PA_KAI_267X389MM},
      {"om_photo-30x40_300x400mm",
       PRINT_PREVIEW_MEDIA_OM_PHOTO_30X40_300X400MM},
      {"om_photo-30x45_300x450mm",
       PRINT_PREVIEW_MEDIA_OM_PHOTO_30X45_300X450MM},
      {"om_photo-35x46_350x460mm",
       PRINT_PREVIEW_MEDIA_OM_PHOTO_35X46_350X460MM},
      {"om_photo-40x60_400x600mm",
       PRINT_PREVIEW_MEDIA_OM_PHOTO_40X60_400X600MM},
      {"om_photo-50x75_500x750mm",
       PRINT_PREVIEW_MEDIA_OM_PHOTO_50X75_500X750MM},
      {"om_photo-50x76_500x760mm",
       PRINT_PREVIEW_MEDIA_OM_PHOTO_50X76_500X760MM},
      {"om_photo-60x90_600x900mm",
       PRINT_PREVIEW_MEDIA_OM_PHOTO_60X90_600X900MM},
      // Duplicate of iso_c6c5_114x229mm.
      {"om_postfix_114x229mm", PRINT_PREVIEW_MEDIA_ISO_C6C5_114X229MM},
      {"om_small-photo_100x150mm",
       PRINT_PREVIEW_MEDIA_OM_SMALL_PHOTO_100X150MM},
      {"om_square-photo_89x89mm", PRINT_PREVIEW_MEDIA_OM_SQUARE_PHOTO_89X89MM},
      {"om_wide-photo_100x200mm", PRINT_PREVIEW_MEDIA_OM_WIDE_PHOTO_100X200MM},
      // Duplicate of iso_c3_324x458mm.
      {"prc_10_324x458mm", PRINT_PREVIEW_MEDIA_ISO_C3_324X458MM},
      {"prc_16k_146x215mm", PRINT_PREVIEW_MEDIA_PRC_16K_146X215MM},
      {"prc_1_102x165mm", PRINT_PREVIEW_MEDIA_PRC_1_102X165MM},
      {"prc_2_102x176mm", PRINT_PREVIEW_MEDIA_PRC_2_102X176MM},
      {"prc_32k_97x151mm", PRINT_PREVIEW_MEDIA_PRC_32K_97X151MM},
      // Duplicate of iso_b6_125x176mm.
      {"prc_3_125x176mm", PRINT_PREVIEW_MEDIA_ISO_B6_125X176MM},
      {"prc_4_110x208mm", PRINT_PREVIEW_MEDIA_PRC_4_110X208MM},
      // Duplicate of iso_dl_110x220mm.
      {"prc_5_110x220mm", PRINT_PREVIEW_MEDIA_ISO_DL_110X220MM},
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

// Generate a human-readable name from a PWG self-describing name.  If
// `pwg_name` is not a valid self-describing media size, return an empty string.
std::string NameForSelfDescribingSize(const std::string& pwg_name) {
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
    return "";
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
        return l10n_util::GetStringFUTF8(PRINT_PREVIEW_MEDIA_DIMENSIONS_INCHES,
                                         base::ASCIIToUTF16(width),
                                         base::ASCIIToUTF16(height));

      case Unit::kMillimeters:
        return l10n_util::GetStringFUTF8(PRINT_PREVIEW_MEDIA_DIMENSIONS_MM,
                                         base::ASCIIToUTF16(width),
                                         base::ASCIIToUTF16(height));
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
      return l10n_util::GetStringFUTF8(
          PRINT_PREVIEW_MEDIA_NAME_WITH_DIMENSIONS_INCHES,
          base::ASCIIToUTF16(clean_name), base::ASCIIToUTF16(width),
          base::ASCIIToUTF16(height));

    case Unit::kMillimeters:
      return l10n_util::GetStringFUTF8(
          PRINT_PREVIEW_MEDIA_NAME_WITH_DIMENSIONS_MM,
          base::ASCIIToUTF16(clean_name), base::ASCIIToUTF16(width),
          base::ASCIIToUTF16(height));
  }
}

}  // namespace

std::string LocalizePaperDisplayName(const std::string& vendor_id) {
  // We can't do anything without a vendor ID.
  if (vendor_id.empty()) {
    return std::string();
  }

  int translation_id = VendorIdToTranslatedId(vendor_id);
  return translation_id < 0 ? NameForSelfDescribingSize(vendor_id)
                            : l10n_util::GetStringUTF8(translation_id);
}

}  // namespace printing
