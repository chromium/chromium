// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/file_icon_util.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/native_theme/native_theme.h"

namespace chromeos {
namespace {

constexpr int kIconDefaultDipSize = 20;

enum class ColorId {
  kBlue,
  kGreen,
  kGrey,
  kRed,
  kYellow,
  kFiletypeGsite,
  kFiletypePpt,
  kFiletypeSites,
  kFiletypeGform,
  kFiletypeExcel,
  kFiletypeWord,
};

struct IconParams {
  const raw_ref<const gfx::VectorIcon> icon;
  ColorId color_id;
};

// TODO(b/280519843): Return ui::ColorId instead of SkColor so we don't need to
// get ColorProvider from the util function.
SkColor ResolveColor(ColorId color_id, bool dark_background) {
  // Changes to this should be reflected in
  // ui/file_manager/file_manager/foreground/css/file_types.css.
  if (chromeos::features::IsJellyrollEnabled()) {
    auto* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
    auto* color_provider = ui::ColorProviderManager::Get().GetColorProviderFor(
        native_theme->GetColorProviderKey(nullptr));
    switch (color_id) {
      case ColorId::kBlue:
        return color_provider->GetColor(cros_tokens::kCrosSysProgress);
      case ColorId::kGreen:
        return color_provider->GetColor(cros_tokens::kCrosSysPositive);
      case ColorId::kGrey:
        return color_provider->GetColor(cros_tokens::kCrosSysOnSurface);
      case ColorId::kRed:
        return color_provider->GetColor(cros_tokens::kCrosSysError);
      case ColorId::kYellow:
        return color_provider->GetColor(cros_tokens::kCrosSysWarning);
      case ColorId::kFiletypePpt:
        return color_provider->GetColor(cros_tokens::kCrosSysFileMsPpt);
      case ColorId::kFiletypeGsite:
      case ColorId::kFiletypeSites:
        return color_provider->GetColor(cros_tokens::kCrosSysFileSite);
      case ColorId::kFiletypeGform:
        return color_provider->GetColor(cros_tokens::kCrosSysFileForm);
      case ColorId::kFiletypeExcel:
        return color_provider->GetColor(cros_tokens::kCrosSysFileMsExcel);
      case ColorId::kFiletypeWord:
        return color_provider->GetColor(cros_tokens::kCrosSysFileMsWord);
    }
  } else {
    switch (color_id) {
      case ColorId::kBlue:
        return cros_styles::ResolveColor(cros_styles::ColorName::kIconColorBlue,
                                         dark_background,
                                         /*use_debug_colors=*/false);
      case ColorId::kGreen:
        return cros_styles::ResolveColor(
            cros_styles::ColorName::kIconColorGreen, dark_background,
            /*use_debug_colors=*/false);
      case ColorId::kGrey:
        return cros_styles::ResolveColor(
            cros_styles::ColorName::kIconColorPrimary, dark_background,
            /*use_debug_colors=*/false);
      case ColorId::kRed:
        return cros_styles::ResolveColor(cros_styles::ColorName::kIconColorRed,
                                         dark_background,
                                         /*use_debug_colors=*/false);
      case ColorId::kYellow:
        return cros_styles::ResolveColor(
            cros_styles::ColorName::kIconColorYellow, dark_background,
            /*use_debug_colors=*/false);
      case ColorId::kFiletypePpt:
        return SkColorSetRGB(0xFF, 0x76, 0x37);
      case ColorId::kFiletypeGsite:
      case ColorId::kFiletypeSites:
        return dark_background ? SkColorSetRGB(0xBC, 0x9E, 0xFF)
                               : SkColorSetRGB(0x79, 0x6E, 0xEE);
      case ColorId::kFiletypeGform:
      case ColorId::kFiletypeExcel:
        // Before refresh23, they are mapped to kGreen.
        return cros_styles::ResolveColor(
            cros_styles::ColorName::kIconColorGreen, dark_background,
            /*use_debug_colors=*/false);
      case ColorId::kFiletypeWord:
        // Before refresh23, it's mapped to kBlue.
        return cros_styles::ResolveColor(cros_styles::ColorName::kIconColorBlue,
                                         dark_background,
                                         /*use_debug_colors=*/false);
    }
  }
}

const std::map<IconType, IconParams>& GetIconTypeToIconParamsMap() {
  // Changes to this map should be reflected in
  // ui/file_manager/file_manager/common/js/file_type.js.
  static const base::NoDestructor<std::map<IconType, IconParams>>
      icon_type_to_icon_params(
          {{IconType::kArchive,
            IconParams{raw_ref(kFiletypeArchiveIcon), ColorId::kGrey}},
           {IconType::kAudio,
            IconParams{raw_ref(kFiletypeAudioIcon), ColorId::kRed}},
           {IconType::kChart,
            IconParams{raw_ref(kFiletypeChartIcon), ColorId::kGreen}},
           {IconType::kDrive,
            IconParams{raw_ref(kFiletypeTeamDriveIcon), ColorId::kGrey}},
           {IconType::kExcel,
            IconParams{raw_ref(kFiletypeExcelIcon), ColorId::kFiletypeExcel}},
           {IconType::kFolder,
            IconParams{raw_ref(kFiletypeFolderIcon), ColorId::kGrey}},
           {IconType::kFolderShared,
            IconParams{raw_ref(kFiletypeSharedIcon), ColorId::kGrey}},
           {IconType::kGdoc,
            IconParams{raw_ref(kFiletypeGdocIcon), ColorId::kBlue}},
           {IconType::kGdraw,
            IconParams{raw_ref(kFiletypeGdrawIcon), ColorId::kRed}},
           {IconType::kGeneric,
            IconParams{raw_ref(kFiletypeGenericIcon), ColorId::kGrey}},
           {IconType::kGform,
            IconParams{raw_ref(kFiletypeGformIcon), ColorId::kFiletypeGform}},
           {IconType::kGmap,
            IconParams{raw_ref(kFiletypeGmapIcon), ColorId::kRed}},
           {IconType::kGsheet,
            IconParams{raw_ref(kFiletypeGsheetIcon), ColorId::kGreen}},
           {IconType::kGsite,
            IconParams{raw_ref(kFiletypeGsiteIcon), ColorId::kFiletypeGsite}},
           {IconType::kGmaillayout,
            IconParams{raw_ref(kFiletypeGmaillayoutIcon), ColorId::kRed}},
           {IconType::kGslide,
            IconParams{raw_ref(kFiletypeGslidesIcon), ColorId::kYellow}},
           {IconType::kGtable,
            IconParams{raw_ref(kFiletypeGtableIcon), ColorId::kGreen}},
           {IconType::kImage,
            IconParams{raw_ref(kFiletypeImageIcon), ColorId::kRed}},
           {IconType::kLinux,
            IconParams{raw_ref(kFiletypeLinuxIcon), ColorId::kGrey}},
           {IconType::kPdf,
            IconParams{raw_ref(kFiletypePdfIcon), ColorId::kRed}},
           {IconType::kPpt,
            IconParams{raw_ref(kFiletypePptIcon), ColorId::kFiletypePpt}},
           {IconType::kScript,
            IconParams{raw_ref(kFiletypeScriptIcon), ColorId::kBlue}},
           {IconType::kSites,
            IconParams{raw_ref(kFiletypeSitesIcon), ColorId::kFiletypeSites}},
           {IconType::kTini,
            IconParams{raw_ref(kFiletypeTiniIcon), ColorId::kBlue}},
           {IconType::kVideo,
            IconParams{raw_ref(kFiletypeVideoIcon), ColorId::kRed}},
           {IconType::kWord,
            IconParams{raw_ref(kFiletypeWordIcon), ColorId::kFiletypeWord}}});
  return *icon_type_to_icon_params;
}

const IconParams& GetIconParamsFromIconType(IconType icon) {
  const auto& icon_type_to_icon_params = GetIconTypeToIconParamsMap();
  const auto& it = icon_type_to_icon_params.find(icon);
  CHECK(it != icon_type_to_icon_params.end(), base::NotFatalUntil::M130);

  return it->second;
}

gfx::ImageSkia GetVectorIconFromIconType(IconType icon,
                                         bool dark_background,
                                         std::optional<int> dip_size) {
  const IconParams& params = GetIconParamsFromIconType(icon);
  const gfx::IconDescription description(
      *params.icon, dip_size.value_or(kIconDefaultDipSize),
      ResolveColor(params.color_id, dark_background));

  return gfx::CreateVectorIcon(description);
}

}  // namespace

namespace internal {

IconType GetIconTypeForPath(const base::FilePath& filepath) {
  // Changes to this map should be reflected in
  // ui/file_manager/base/gn/file_types.json5
  static const auto extension_to_icon =
      base::MakeFixedFlatMap<std::string_view, IconType>({
          // Image
          {".JPEG", IconType::kImage},
          {".JPG", IconType::kImage},
          {".BMP", IconType::kImage},
          {".GIF", IconType::kImage},
          {".ICO", IconType::kImage},
          {".PNG", IconType::kImage},
          {".WEBP", IconType::kImage},
          {".TIFF", IconType::kImage},
          {".TIF", IconType::kImage},
          {".SVG", IconType::kImage},

          // Raw
          {".ARW", IconType::kImage},
          {".CR2", IconType::kImage},
          {".DNG", IconType::kImage},
          {".NEF", IconType::kImage},
          {".NRW", IconType::kImage},
          {".ORF", IconType::kImage},
          {".RAF", IconType::kImage},
          {".RW2", IconType::kImage},

          // Video
          {".3GP", IconType::kVideo},
          {".3GPP", IconType::kVideo},
          {".AVI", IconType::kVideo},
          {".MOV", IconType::kVideo},
          {".MKV", IconType::kVideo},
          {".MP4", IconType::kVideo},
          {".M4V", IconType::kVideo},
          {".MPG", IconType::kVideo},
          {".MPEG", IconType::kVideo},
          {".MPG4", IconType::kVideo},
          {".MPEG4", IconType::kVideo},
          {".OGM", IconType::kVideo},
          {".OGV", IconType::kVideo},
          {".OGX", IconType::kVideo},
          {".WEBM", IconType::kVideo},

          // Audio
          {".AMR", IconType::kAudio},
          {".FLAC", IconType::kAudio},
          {".MP3", IconType::kAudio},
          {".M4A", IconType::kAudio},
          {".OGA", IconType::kAudio},
          {".OGG", IconType::kAudio},
          {".WAV", IconType::kAudio},

          // Text
          {".TXT", IconType::kGeneric},

          // Archive
          {".7Z", IconType::kArchive},
          {".BZ", IconType::kArchive},
          {".BZ2", IconType::kArchive},
          {".CRX", IconType::kArchive},
          {".GZ", IconType::kArchive},
          {".ISO", IconType::kArchive},
          {".LZ", IconType::kArchive},
          {".LZMA", IconType::kArchive},
          {".LZO", IconType::kArchive},
          {".RAR", IconType::kArchive},
          {".TAR", IconType::kArchive},
          {".TAZ", IconType::kArchive},
          {".TB2", IconType::kArchive},
          {".TBZ", IconType::kArchive},
          {".TBZ2", IconType::kArchive},
          {".TGZ", IconType::kArchive},
          {".TLZ", IconType::kArchive},
          {".TLZMA", IconType::kArchive},
          {".TXZ", IconType::kArchive},
          {".TZ", IconType::kArchive},
          {".TZ2", IconType::kArchive},
          {".TZST", IconType::kArchive},
          {".XZ", IconType::kArchive},
          {".Z", IconType::kArchive},
          {".ZIP", IconType::kArchive},

          // Hosted doc
          {".GDOC", IconType::kGdoc},
          {".GSHEET", IconType::kGsheet},
          {".GSLIDES", IconType::kGslide},
          {".GDRAW", IconType::kGdraw},
          {".GTABLE", IconType::kGtable},
          {".GLINK", IconType::kGeneric},
          {".GFORM", IconType::kGform},
          {".GMAPS", IconType::kGmap},
          {".GSITE", IconType::kGsite},
          {".GMAILLAYOUT", IconType::kGmaillayout},

          // Other
          {".PDF", IconType::kPdf},
          {".HTM", IconType::kGeneric},
          {".HTML", IconType::kGeneric},
          {".MHT", IconType::kGeneric},
          {".MHTM", IconType::kGeneric},
          {".MHTML", IconType::kGeneric},
          {".SHTML", IconType::kGeneric},
          {".XHT", IconType::kGeneric},
          {".XHTM", IconType::kGeneric},
          {".XHTML", IconType::kGeneric},
          {".DOC", IconType::kWord},
          {".DOCX", IconType::kWord},
          {".PPT", IconType::kPpt},
          {".PPTX", IconType::kPpt},
          {".XLS", IconType::kExcel},
          {".XLSM", IconType::kExcel},
          {".XLSX", IconType::kExcel},
          {".TINI", IconType::kTini},
      });

  const auto it =
      extension_to_icon.find(base::ToUpperASCII(filepath.Extension()));
  if (it != extension_to_icon.end()) {
    return it->second;
  } else {
    return IconType::kGeneric;
  }
}

IconType GetIconTypeFromString(const std::string& icon_type_string) {
  static const base::NoDestructor<std::map<std::string, IconType>>
      type_string_to_icon_type({{"archive", IconType::kArchive},
                                {"audio", IconType::kAudio},
                                {"chart", IconType::kChart},
                                {"excel", IconType::kExcel},
                                {"drive", IconType::kDrive},
                                {"folder", IconType::kFolder},
                                {"gdoc", IconType::kGdoc},
                                {"gdraw", IconType::kGdraw},
                                {"generic", IconType::kGeneric},
                                {"gform", IconType::kGform},
                                {"gmap", IconType::kGmap},
                                {"gsheet", IconType::kGsheet},
                                {"gsite", IconType::kGsite},
                                {"gmaillayout", IconType::kGmaillayout},
                                {"gslides", IconType::kGslide},
                                {"gtable", IconType::kGtable},
                                {"image", IconType::kImage},
                                {"linux", IconType::kLinux},
                                {"pdf", IconType::kPdf},
                                {"ppt", IconType::kPpt},
                                {"script", IconType::kScript},
                                {"shared", IconType::kFolderShared},
                                {"sites", IconType::kSites},
                                {"tini", IconType::kTini},
                                {"video", IconType::kVideo},
                                {"word", IconType::kWord}});

  const auto& icon_it = type_string_to_icon_type->find(icon_type_string);
  if (icon_it != type_string_to_icon_type->end())
    return icon_it->second;
  return IconType::kGeneric;
}

}  // namespace internal

const gfx::VectorIcon& GetIconForPath(const base::FilePath& filepath) {
  return *GetIconParamsFromIconType(internal::GetIconTypeForPath(filepath))
              .icon;
}

gfx::ImageSkia GetIconForPath(const base::FilePath& filepath,
                              bool dark_background,
                              std::optional<int> dip_size) {
  return GetVectorIconFromIconType(internal::GetIconTypeForPath(filepath),
                                   dark_background, dip_size);
}

gfx::ImageSkia GetChipIconForPath(const base::FilePath& filepath,
                                  bool dark_background) {
  return GetIconForPath(filepath, dark_background);
}

const gfx::VectorIcon& GetIconFromType(const std::string& icon_type) {
  return *GetIconParamsFromIconType(internal::GetIconTypeFromString(icon_type))
              .icon;
}

gfx::ImageSkia GetIconFromType(const std::string& icon_type,
                               bool dark_background) {
  return GetVectorIconFromIconType(internal::GetIconTypeFromString(icon_type),
                                   dark_background, std::nullopt);
}

gfx::ImageSkia GetIconFromType(IconType icon_type,
                               bool dark_background,
                               std::optional<int> dip_size) {
  return GetVectorIconFromIconType(icon_type, dark_background, dip_size);
}

SkColor GetIconColorForPath(const base::FilePath& filepath,
                            bool dark_background) {
  const auto& icon_type = internal::GetIconTypeForPath(filepath);
  const auto& icon_type_to_icon_params = GetIconTypeToIconParamsMap();
  const auto& it = icon_type_to_icon_params.find(icon_type);
  CHECK(it != icon_type_to_icon_params.end(), base::NotFatalUntil::M130);

  return ResolveColor(it->second.color_id, dark_background);
}

}  // namespace chromeos
