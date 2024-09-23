// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font/public/cpp/font_loader.h"

#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/services/font/font_service_app.h"
#include "components/services/font/public/mojom/font_service.mojom.h"
#include "pdf/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkFontStyle.h"

#if BUILDFLAG(ENABLE_PDF)
#include <ft2build.h>

#include <freetype/freetype.h>
#include "third_party/pdfium/public/fpdf_sysfontinfo.h"
#endif  // BUILDFLAG(ENABLE_PDF)

namespace font_service {
namespace {

bool IsInTestFontDirectory(const base::FilePath& path) {
  const base::FilePath kTestFontsDir(
      FILE_PATH_LITERAL("./third_party/test_fonts"));
  return kTestFontsDir.IsParent(path);
}

#if BUILDFLAG(ENABLE_PDF)
std::string GetPostscriptNameFromFile(base::File& font_file) {
  int64_t file_size = font_file.GetLength();
  if (!file_size)
    return "";

  std::vector<char> file_contents(file_size);
  CHECK(font_file.ReadAndCheck(0, base::as_writable_byte_span(file_contents)));

  std::string font_family_name;
  FT_Library library;
  FT_Init_FreeType(&library);
  FT_Face font_face;
  FT_Open_Args open_args = {FT_OPEN_MEMORY,
                            reinterpret_cast<FT_Byte*>(file_contents.data()),
                            static_cast<FT_Long>(file_size)};
  CHECK_EQ(FT_Err_Ok, FT_Open_Face(library, &open_args, 0, &font_face));
  font_family_name = FT_Get_Postscript_Name(font_face);
  FT_Done_Face(font_face);
  FT_Done_FreeType(library);
  return font_family_name;
}
#endif  // BUILDFLAG(ENABLE_PDF)

mojo::PendingRemote<mojom::FontService> ConnectToBackgroundFontService() {
  mojo::PendingRemote<mojom::FontService> remote;
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     [](mojo::PendingReceiver<mojom::FontService> receiver) {
                       static base::NoDestructor<FontServiceApp> service;
                       service->BindReceiver(std::move(receiver));
                     },
                     remote.InitWithNewPipeAndPassReceiver()));
  return remote;
}

class FontLoaderTest : public testing::Test {
 public:
  FontLoaderTest() = default;

  FontLoaderTest(const FontLoaderTest&) = delete;
  FontLoaderTest& operator=(const FontLoaderTest&) = delete;

  ~FontLoaderTest() override = default;

 protected:
  FontLoader* font_loader() { return &font_loader_; }

 private:
  base::test::TaskEnvironment task_environment_;
  FontLoader font_loader_{ConnectToBackgroundFontService()};
};

}  // namespace

TEST_F(FontLoaderTest, BasicMatchingTest) {
  SkFontStyle styles[] = {
      SkFontStyle(SkFontStyle::kNormal_Weight, SkFontStyle::kNormal_Width,
                  SkFontStyle::kUpright_Slant),
      SkFontStyle(SkFontStyle::kBold_Weight, SkFontStyle::kNormal_Width,
                  SkFontStyle::kUpright_Slant),
      SkFontStyle(SkFontStyle::kBold_Weight, SkFontStyle::kNormal_Width,
                  SkFontStyle::kItalic_Slant)};
  // See kFontsConfTemplate[] in fontconfig_util_linux.cc for details of which
  // fonts can be picked. Arial, Times New Roman and Courier New are aliased to
  // Arimos, Tinos and Cousine ChromeOS open source alternatives when FontConfig
  // is set up for testing.
  std::vector<std::vector<std::string>> family_names_expected_names = {
      {"Arial", "Arimo"},
      {"Times New Roman", "Tinos"},
      {"Courier New", "Cousine"}};
  for (std::vector<std::string> request_family_name :
       family_names_expected_names) {
    for (auto& request_style : styles) {
      SkFontConfigInterface::FontIdentity font_identity;
      SkFontStyle result_style;
      SkString result_family_name;
      font_loader()->matchFamilyName(request_family_name[0].c_str(),
                                     request_style, &font_identity,
                                     &result_family_name, &result_style);
      EXPECT_EQ(request_family_name[1],
                std::string(result_family_name.c_str()));
      EXPECT_TRUE(
          IsInTestFontDirectory(base::FilePath(font_identity.fString.c_str())));
      EXPECT_EQ(result_style, request_style);
    }
  }
}

TEST_F(FontLoaderTest, NotFoundTest) {
  std::string request_family_name = {"IMPROBABLE_FONT_NAME"};
  SkFontConfigInterface::FontIdentity font_identity;
  SkFontStyle result_style;
  SkString result_family_name;
  font_loader()->matchFamilyName(request_family_name.c_str(), SkFontStyle(),
                                 &font_identity, &result_family_name,
                                 &result_style);
  EXPECT_EQ("", std::string(result_family_name.c_str()));
  EXPECT_EQ(0u, font_identity.fID);
  EXPECT_EQ("", std::string(font_identity.fString.c_str()));
}

TEST_F(FontLoaderTest, EmptyFontName) {
  std::string request_family_name = {""};
  std::string kDefaultFontName = "DejaVu Sans";
  SkFontConfigInterface::FontIdentity font_identity;
  SkFontStyle result_style;
  SkString result_family_name;
  font_loader()->matchFamilyName(request_family_name.c_str(), SkFontStyle(),
                                 &font_identity, &result_family_name,
                                 &result_style);
  EXPECT_EQ(kDefaultFontName, std::string(result_family_name.c_str()));
  EXPECT_TRUE(
      IsInTestFontDirectory(base::FilePath(font_identity.fString.c_str())));
}

TEST_F(FontLoaderTest, CharacterFallback) {
  std::pair<uint32_t, std::string> fallback_characters_families[] = {
      // A selection of character hitting fonts from the third_party/test_fonts
      // portfolio.
      {0x662F /* CJK UNIFIED IDEOGRAPH-662F */, "Noto Sans CJK JP"},
      {0x1780 /* KHMER LETTER KA */, "Noto Sans Khmer"},
      {0xA05 /* GURMUKHI LETTER A */, "Lohit Gurmukhi"},
      {0xB85 /* TAMIL LETTER A */, "Lohit Tamil"},
      {0x904 /* DEVANAGARI LETTER SHORT A */, "Lohit Devanagari"},
      {0x985 /* BENGALI LETTER A */, "Mukti Narrow"},
      // Tests for not finding fallback:
      {0x13170 /* EGYPTIAN HIEROGLYPH G042 */, ""},
      {0x1817 /* MONGOLIAN DIGIT SEVEN */, ""}};
  for (auto& character_family : fallback_characters_families) {
    mojom::FontIdentityPtr font_identity;
    std::string result_family_name;
    bool is_bold;
    bool is_italic;
    font_loader()->FallbackFontForCharacter(
        std::move(character_family.first), "en_US", &font_identity,
        &result_family_name, &is_bold, &is_italic);
    EXPECT_EQ(result_family_name, character_family.second);
    EXPECT_FALSE(is_bold);
    EXPECT_FALSE(is_italic);
    if (character_family.second.size()) {
      EXPECT_TRUE(IsInTestFontDirectory(font_identity->filepath));
    } else {
      EXPECT_TRUE(font_identity->filepath.empty());
      EXPECT_EQ(result_family_name, "");
    }
  }
}

TEST_F(FontLoaderTest, RenderStyleForStrike) {
  // Use FontConfig configured test font aliases from kFontsConfTemplate in
  // fontconfig_util_linux.cc.
  std::pair<std::string, mojom::FontRenderStylePtr> families_styles[] = {
      {"NonAntiAliasedSans",
       mojom::FontRenderStyle::New(
           mojom::RenderStyleSwitch::ON, mojom::RenderStyleSwitch::OFF,
           mojom::RenderStyleSwitch::ON, 3, mojom::RenderStyleSwitch::OFF,
           mojom::RenderStyleSwitch::OFF, mojom::RenderStyleSwitch::OFF)},
      {"SlightHintedGeorgia",
       mojom::FontRenderStyle::New(
           mojom::RenderStyleSwitch::ON, mojom::RenderStyleSwitch::OFF,
           mojom::RenderStyleSwitch::ON, 1, mojom::RenderStyleSwitch::ON,
           mojom::RenderStyleSwitch::OFF, mojom::RenderStyleSwitch::OFF)},
      {"NonHintedSans",
       mojom::FontRenderStyle::New(
           mojom::RenderStyleSwitch::ON, mojom::RenderStyleSwitch::OFF,
           mojom::RenderStyleSwitch::OFF, 0, mojom::RenderStyleSwitch::ON,
           mojom::RenderStyleSwitch::OFF, mojom::RenderStyleSwitch::OFF)},
      {"AutohintedSerif",
       mojom::FontRenderStyle::New(
           mojom::RenderStyleSwitch::ON, mojom::RenderStyleSwitch::ON,
           mojom::RenderStyleSwitch::ON, 2, mojom::RenderStyleSwitch::ON,
           mojom::RenderStyleSwitch::OFF, mojom::RenderStyleSwitch::OFF)},
      {"HintedSerif",
       mojom::FontRenderStyle::New(
           mojom::RenderStyleSwitch::ON, mojom::RenderStyleSwitch::OFF,
           mojom::RenderStyleSwitch::ON, 2, mojom::RenderStyleSwitch::ON,
           mojom::RenderStyleSwitch::OFF, mojom::RenderStyleSwitch::OFF)},
      {"FullAndAutoHintedSerif",
       mojom::FontRenderStyle::New(
           mojom::RenderStyleSwitch::ON, mojom::RenderStyleSwitch::ON,
           mojom::RenderStyleSwitch::ON, 3, mojom::RenderStyleSwitch::ON,
           mojom::RenderStyleSwitch::OFF, mojom::RenderStyleSwitch::OFF)},
      {"SubpixelEnabledArial",
       mojom::FontRenderStyle::New(
           mojom::RenderStyleSwitch::ON, mojom::RenderStyleSwitch::OFF,
           mojom::RenderStyleSwitch::ON, 3, mojom::RenderStyleSwitch::ON,
           mojom::RenderStyleSwitch::ON, mojom::RenderStyleSwitch::OFF)},
      {"SubpixelDisabledArial",
       mojom::FontRenderStyle::New(
           mojom::RenderStyleSwitch::ON, mojom::RenderStyleSwitch::OFF,
           mojom::RenderStyleSwitch::ON, 3, mojom::RenderStyleSwitch::ON,
           mojom::RenderStyleSwitch::OFF, mojom::RenderStyleSwitch::OFF)}};

  for (auto& family_style : families_styles) {
    mojom::FontRenderStylePtr result_style;
    font_loader()->FontRenderStyleForStrike(std::move(family_style.first), 16,
                                            false, false, 1.0, &result_style);
    EXPECT_TRUE(result_style.Equals(family_style.second));
  }
}

#if BUILDFLAG(ENABLE_PDF)
TEST_F(FontLoaderTest, PdfFallback) {
  std::tuple<std::string, uint32_t, std::string> family_charset_expectations[] =
      {
          {"", FXFONT_SHIFTJIS_CHARSET, "DejaVuSans"},
          {"", FXFONT_THAI_CHARSET, "Garuda"},
          {"", FXFONT_GB2312_CHARSET, "DejaVuSans"},
          {"", FXFONT_GREEK_CHARSET, "DejaVuSans"},
          {"Arial", FXFONT_DEFAULT_CHARSET, "Arimo-Regular"},
          {"Times New Roman", FXFONT_DEFAULT_CHARSET, "Tinos-Regular"},
          {"Courier New", FXFONT_DEFAULT_CHARSET, "Cousine-Regular"},
      };
  for (auto& family_charset_expectation : family_charset_expectations) {
    base::File result_file;
    font_loader()->MatchFontWithFallback(
        std::move(std::get<0>(family_charset_expectation)), false, false,
        std::move(std::get<1>(family_charset_expectation)), 0, &result_file);
    EXPECT_TRUE(result_file.IsValid());
    EXPECT_EQ(GetPostscriptNameFromFile(result_file),
              std::get<2>(family_charset_expectation));
  }
}
#endif  // BUILDFLAG(ENABLE_PDF)

TEST_F(FontLoaderTest, LocalMatching) {
  // The following fonts are ensured to be available by the test harnesses
  // global FontConfig setup which makes the fonts in third_party/test_fonts
  // available. (See SetUpFontConfig() in TestSuite::Initialize().
  std::string postscript_names_test_fonts[] = {"Ahem",
                                               "Arimo-Bold",
                                               "Arimo-BoldItalic",
                                               "Arimo-Italic",
                                               "Arimo-Regular",
                                               "Cousine-Bold",
                                               "Cousine-BoldItalic",
                                               "Cousine-Italic",
                                               "Cousine-Regular",
                                               "DejaVuSans",
                                               "DejaVuSans-Bold",
                                               "GardinerModBug",
                                               "GardinerModCat",
                                               "Garuda",
                                               "Gelasio-Bold",
                                               "Gelasio-BoldItalic",
                                               "Gelasio-Italic",
                                               "Gelasio-Regular",
                                               "Lohit-Devanagari",
                                               "Lohit-Gurmukhi",
                                               "Lohit-Tamil",
                                               "NotoSansKhmer-Regular",
                                               "Tinos-Bold",
                                               "Tinos-BoldItalic",
                                               "Tinos-Italic",
                                               "Tinos-Regular",
                                               "muktinarrow"};
  std::string full_font_names_test_fonts[] = {"Ahem",
                                              "Arimo Bold Italic",
                                              "Arimo Bold",
                                              "Arimo Italic",
                                              "Arimo Regular",
                                              "Cousine Bold Italic",
                                              "Cousine Bold",
                                              "Cousine Italic",
                                              "Cousine Regular",
                                              "DejaVu Sans Bold",
                                              "DejaVu Sans",
                                              "GardinerMod",
                                              "Garuda",
                                              "Gelasio Bold Italic",
                                              "Gelasio Bold",
                                              "Gelasio Italic",
                                              "Gelasio Regular",
                                              "Lohit Devanagari",
                                              "Lohit Gurmukhi",
                                              "Lohit Tamil",
                                              "Mukti",
                                              "Mukti Narrow",
                                              "Noto Sans Khmer Regular",
                                              "Tinos Bold Italic",
                                              "Tinos Bold",
                                              "Tinos Italic",
                                              "Tinos Regular"};

  auto match_unique_names = [this](auto& font_list) {
    for (auto unique_font_name : font_list) {
      mojom::FontIdentityPtr font_identity;
      EXPECT_TRUE(font_loader()->MatchFontByPostscriptNameOrFullFontName(
          unique_font_name, &font_identity));
      EXPECT_FALSE(font_identity.is_null());
      EXPECT_TRUE(IsInTestFontDirectory(font_identity->filepath));
    }
  };
  match_unique_names(full_font_names_test_fonts);
  match_unique_names(postscript_names_test_fonts);
}

TEST_F(FontLoaderTest, LocalMatchingExpectNoMatchForFamilyNames) {
  std::string family_names_expect_no_match[] = {"Arimo", "Cousine",   "Gelasio",
                                                "Lohit", "Noto Sans", "Tinos"};
  for (auto& family_name : family_names_expect_no_match) {
    mojom::FontIdentityPtr font_identity;
    EXPECT_FALSE(font_loader()->MatchFontByPostscriptNameOrFullFontName(
        family_name, &font_identity));
    EXPECT_TRUE(font_identity.is_null());
  }
}

TEST_F(FontLoaderTest, RejectNonUtf8) {
  const char* invalid_utf8_font_names[] = {
      // Trailing U+FDD0 U+FDD1 U+FDD2 U+FDD3
      "FontNameWithNonCharacters\xEF\xB7\x90\x20\xEF\xB7\x91\x20\xEF\xB7"
      "\x92\x20\xEF\xB7\x93",
      "InvalidBytes\xfe\xff"};
  for (std::string invalid_font_name : invalid_utf8_font_names) {
    mojom::FontIdentityPtr font_identity;
    EXPECT_FALSE(font_loader()->MatchFontByPostscriptNameOrFullFontName(
        invalid_font_name, &font_identity));
    EXPECT_TRUE(font_identity.is_null());
  }
}

}  // namespace font_service
