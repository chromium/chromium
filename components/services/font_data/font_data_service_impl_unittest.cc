// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font_data/font_data_service_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/services/font_data/public/mojom/font_data_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/direct_write.h"
#endif

namespace font_data_service {

namespace {

class TestFontDataService : public FontDataServiceImpl {
 public:
  TestFontDataService() = default;
  ~TestFontDataService() override = default;
  TestFontDataService(const TestFontDataService&) = delete;
  TestFontDataService& operator=(const TestFontDataService&) = delete;

  std::tuple<base::File, uint64_t> GetFileHandle(
      SkTypeface& typeface) override {
    if (use_memory_fallback_) {
      // Return an empty file handle to simulate the fallback.
      return {base::File(), 0UL};
    }
    return FontDataServiceImpl::GetFileHandle(typeface);
  }

  void set_use_memory_fallback(bool fallback) {
    use_memory_fallback_ = fallback;
  }

  bool CheckMatchesRequiredStyleForTesting(
      const SkFontStyle& actual_style,
      const std::string& requested_family_name,
      const SkFontStyle& requested_style) {
    return CheckMatchesRequiredStyle(actual_style, requested_family_name,
                                     requested_style);
  }

 private:
  bool use_memory_fallback_ = false;
};

class FontDataServiceImplUnitTest : public testing::Test {
 protected:
  FontDataServiceImplUnitTest()
      : receiver_(&impl_, font_service_.BindNewPipeAndPassReceiver()) {}
  ~FontDataServiceImplUnitTest() override = default;

  base::test::SingleThreadTaskEnvironment environment_;
  mojo::Remote<mojom::FontDataService> font_service_;
  TestFontDataService impl_;
  mojo::Receiver<mojom::FontDataService> receiver_;
};

mojom::TypefaceStylePtr CreateTypefaceStyle(int weight,
                                            int width,
                                            mojom::TypefaceSlant slant) {
  mojom::TypefaceStylePtr style(mojom::TypefaceStyle::New());
  style->weight = weight;
  style->width = width;
  style->slant = slant;
  return style;
}

#if BUILDFLAG(IS_WIN)
void ExpectValidTypefaceData(const mojom::TypefaceDataPtr& typeface_data) {
  ASSERT_TRUE(typeface_data);
  if (typeface_data->is_font_file()) {
    EXPECT_TRUE(typeface_data->get_font_file()->file_handle.IsValid());
    return;
  }

  if (typeface_data->is_region()) {
    EXPECT_TRUE(typeface_data->get_region().IsValid());
    return;
  }

  ADD_FAILURE() << "Unexpected TypefaceData variant";
}
#endif

// The CheckMatchesRequiredStyles workaround is only implemented on Windows.
#if BUILDFLAG(IS_WIN)
TEST_F(FontDataServiceImplUnitTest, SegoeDoesntRequireStyle) {
  // A matching style is always valid.
  EXPECT_TRUE(impl_.CheckMatchesRequiredStyleForTesting(
      SkFontStyle::Normal(), "Segoe UI", SkFontStyle::Normal()));
  EXPECT_TRUE(impl_.CheckMatchesRequiredStyleForTesting(
      SkFontStyle::Bold(), "Segoe UI", SkFontStyle::Bold()));
  // Segoe isn't one of gill sans, open sans, or helvetica, so the required
  // styles workaround doesn't apply to it. A requested "normal" match that
  // finds a bold face is valid.
  EXPECT_TRUE(impl_.CheckMatchesRequiredStyleForTesting(
      SkFontStyle::Bold(), "Segoe UI", SkFontStyle::Normal()));
  EXPECT_TRUE(impl_.CheckMatchesRequiredStyleForTesting(
      SkFontStyle::Italic(), "Segoe UI", SkFontStyle::Normal()));
}

TEST_F(FontDataServiceImplUnitTest, FamiliesThatRequireMatchingStyles) {
  static const std::string kFamiliesWithRequiredStyles[] = {
      "gill sans",
      "helvetica",
      "open sans",
  };

  for (const auto& family : kFamiliesWithRequiredStyles) {
    // If the host system doesn't have a regular face for this family, verify
    // that our new logic correctly rejects the family for both Normal and Bold
    // requests.
    sk_sp<SkTypeface> regular =
        skia::DefaultFontMgr()->matchFamilyStyle(family.c_str(), SkFontStyle());
    if (!regular || regular->fontStyle() != SkFontStyle::Normal()) {
      // Even if we find a "closest match" face (like Ultra Bold), it should be
      // rejected because the family is missing its Regular anchor.
      if (regular) {
        EXPECT_FALSE(impl_.CheckMatchesRequiredStyleForTesting(
            regular->fontStyle(), family, SkFontStyle::Normal()));
        EXPECT_FALSE(impl_.CheckMatchesRequiredStyleForTesting(
            regular->fontStyle(), family, SkFontStyle::Bold()));
      }
      continue;
    }

    // A matching style is always valid.
    EXPECT_TRUE(impl_.CheckMatchesRequiredStyleForTesting(
        SkFontStyle::Normal(), family, SkFontStyle::Normal()));
    EXPECT_TRUE(impl_.CheckMatchesRequiredStyleForTesting(
        SkFontStyle::Bold(), family, SkFontStyle::Bold()));

    // If the found face's style doesn't match the requested one, the match
    // isn't used.
    EXPECT_FALSE(impl_.CheckMatchesRequiredStyleForTesting(
        SkFontStyle::Bold(), family, SkFontStyle::Normal()));

    // A "Normal" match is always valid
    EXPECT_TRUE(impl_.CheckMatchesRequiredStyleForTesting(
        SkFontStyle::Normal(), family, SkFontStyle::Bold()));

    SkFontStyle extra_bold(SkFontStyle::kExtraBold_Weight,
                           SkFontStyle::kNormal_Width,
                           SkFontStyle::kUpright_Slant);
    SkFontStyle extra_light(SkFontStyle::kExtraLight_Weight,
                            SkFontStyle::kNormal_Width,
                            SkFontStyle::kUpright_Slant);
    SkFontStyle light(SkFontStyle::kLight_Weight, SkFontStyle::kNormal_Width,
                      SkFontStyle::kUpright_Slant);

    // A match doesn't require the exact same weight, only that the direction of
    // the match is the same as the direction of the request. For instance, an
    // extra bold face is suitable for a bold request.
    EXPECT_TRUE(impl_.CheckMatchesRequiredStyleForTesting(extra_bold, family,
                                                          SkFontStyle::Bold()));
    EXPECT_TRUE(
        impl_.CheckMatchesRequiredStyleForTesting(light, family, extra_light));

    // The above is also true for width
    SkFontStyle condensed(SkFontStyle::kNormal_Weight,
                          SkFontStyle::kCondensed_Width,
                          SkFontStyle::kUpright_Slant);
    SkFontStyle extra_condensed(SkFontStyle::kNormal_Weight,
                                SkFontStyle::kExtraCondensed_Width,
                                SkFontStyle::kUpright_Slant);
    SkFontStyle expanded(SkFontStyle::kNormal_Weight,
                         SkFontStyle::kExpanded_Width,
                         SkFontStyle::kUpright_Slant);

    EXPECT_TRUE(impl_.CheckMatchesRequiredStyleForTesting(extra_condensed,
                                                          family, condensed));
    EXPECT_FALSE(
        impl_.CheckMatchesRequiredStyleForTesting(condensed, family, expanded));

    // Slant must match exactly.
    EXPECT_FALSE(impl_.CheckMatchesRequiredStyleForTesting(
        SkFontStyle::Italic(), family, SkFontStyle::Normal()));
  }
}

#endif

TEST_F(FontDataServiceImplUnitTest, MatchFamilyName) {
  mojom::MatchFamilyNameResultPtr out_result;
#if BUILDFLAG(IS_WIN)
  std::string family_name = "Segoe UI";
#else
  std::string family_name = "Arimo";
#endif
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 0u);

  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 0u);
  EXPECT_TRUE(out_result->typeface_data->is_font_file());
  EXPECT_TRUE(
      out_result->typeface_data->get_font_file()->file_handle.IsValid());
#else
  // For now, on Linux/ChromeOS we always hit the memory region fallback, and
  // therefore also adds to the cache.
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 1u);
  EXPECT_TRUE(out_result->typeface_data->is_region());
  EXPECT_TRUE(out_result->typeface_data->get_region().IsValid());
#endif
}

TEST_F(FontDataServiceImplUnitTest, MatchFamilyNameMemoryCacheSize) {
  mojom::MatchFamilyNameResultPtr out_result;
#if BUILDFLAG(IS_WIN)
  std::string family_name = "Segoe UI";
#else
  std::string family_name = "Arimo";
#endif
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 0u);
  impl_.set_use_memory_fallback(true);

  // There should be one entry added to the cache.
  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
  EXPECT_TRUE(out_result->typeface_data->is_region());
  EXPECT_TRUE(out_result->typeface_data->get_region().IsValid());
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 1u);

  // Call with the same family name and style. Cache should stay the same
  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 1u);

  // Call with a different family name. Cache should increase.
#if BUILDFLAG(IS_WIN)
  // Bahnschrift is a font with 2 variations. Check result for 2 coordinates.
  family_name = "Bahnschrift";
#else
  family_name = "Tinos";
#endif
  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 2u);
#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/462090356): Find an available font in Linux/ChromeOS with
  // multiples axes.
  EXPECT_EQ(out_result->variation_position->coordinateCount, 2u);
  EXPECT_EQ(out_result->variation_position->coordinates.size(), 2u);
#endif

  // Call with a different font style. Cache should increase.
  font_service_->MatchFamilyName(
      family_name, CreateTypefaceStyle(600, 5, mojom::TypefaceSlant::kOblique),
      &out_result);
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 3u);

  // Call with a gibberish family name. Cache should be the same. Result should
  // be nullptr.
  font_service_->MatchFamilyName(
      "not a real font",
      CreateTypefaceStyle(600, 5, mojom::TypefaceSlant::kOblique), &out_result);
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 3u);
  EXPECT_EQ(out_result.get(), nullptr);
}

// The Linux/ChromeOS SkFontMgr doesn't support MatchFamilyStyleCharacter().
#if BUILDFLAG(IS_WIN)
TEST_F(FontDataServiceImplUnitTest, MatchFamilyNameCharacterNoLanguageTags) {
  mojom::MatchFamilyNameResultPtr out_result;
  std::string family_name = "Segoe UI";
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 0u);
  SkUnichar uni_char = 0x0041;  // 'A'

  font_service_->MatchFamilyNameCharacter(
      family_name, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      {}, uni_char, &out_result);
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 0u);
  EXPECT_TRUE(out_result->typeface_data->is_font_file());
  EXPECT_TRUE(
      out_result->typeface_data->get_font_file()->file_handle.IsValid());
}
#endif

// The Linux/ChromeOS SkFontMgr doesn't support MatchFamilyStyleCharacter().
#if BUILDFLAG(IS_WIN)
TEST_F(FontDataServiceImplUnitTest, MatchFamilyNameCharacterWithLanguageTags) {
  mojom::MatchFamilyNameResultPtr out_result;
  std::string family_name = "Segoe UI";
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 0u);
  SkUnichar uni_char = 0x0041;  // 'A'

  font_service_->MatchFamilyNameCharacter(
      family_name, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      {"zh"}, uni_char, &out_result);
  EXPECT_EQ(impl_.GetCacheSizeForTesting(), 0u);
  EXPECT_TRUE(out_result->typeface_data->is_font_file());
  EXPECT_TRUE(
      out_result->typeface_data->get_font_file()->file_handle.IsValid());
}
#endif

// The Linux/ChromeOS SkFontMgr doesn't support countFamilies().
#if BUILDFLAG(IS_WIN)
TEST_F(FontDataServiceImplUnitTest, GetAllFamilyNames) {
  std::vector<std::string> out_result;
  font_service_->GetAllFamilyNames(&out_result);

  // This tests that GetAllFamilyNames returns the actual font family names
  // installed on the system. There's no guarantee that these are going to be
  // stable across systems, so just assert that the function is returning
  // something, and that the names are non-empty.
  EXPECT_GT(out_result.size(), 0UL);
  for (const auto& family_name : out_result) {
    EXPECT_GT(family_name.size(), 0UL);
  }
}
#endif

TEST_F(FontDataServiceImplUnitTest, LegacyMakeTypefaceNullFamilyName) {
  mojom::MatchFamilyNameResultPtr out_result;

  // LegacyMakeTypeface should return the default font if `family_name` is null.
  font_service_->LegacyMakeTypeface(
      std::nullopt, CreateTypefaceStyle(400, 5, mojom::TypefaceSlant::kRoman),
      &out_result);
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(out_result->typeface_data->is_font_file());
  EXPECT_TRUE(
      out_result->typeface_data->get_font_file()->file_handle.IsValid());
#else
  // For now, on Linux/ChromeOS we always hit the memory region fallback.
  EXPECT_TRUE(out_result->typeface_data->is_region());
  EXPECT_TRUE(out_result->typeface_data->get_region().IsValid());
#endif
}

#if BUILDFLAG(IS_WIN)
TEST_F(FontDataServiceImplUnitTest, MatchLocalFont_ByPostScriptName) {
  mojom::MatchFamilyNameResultPtr out_result;
  font_service_->MatchLocalFont("ArialMT", &out_result);
  ASSERT_TRUE(out_result);
  ExpectValidTypefaceData(out_result->typeface_data);
}

TEST_F(FontDataServiceImplUnitTest, MatchLocalFont_DifferentFontByPostScript) {
  mojom::MatchFamilyNameResultPtr out_result;
  font_service_->MatchLocalFont("TimesNewRomanPSMT", &out_result);
  ASSERT_TRUE(out_result);
  ExpectValidTypefaceData(out_result->typeface_data);
}

TEST_F(FontDataServiceImplUnitTest, MatchLocalFont_BoldByFullName) {
  mojom::MatchFamilyNameResultPtr bold_result;
  font_service_->MatchLocalFont("Arial Bold", &bold_result);
  ASSERT_TRUE(bold_result);
  ExpectValidTypefaceData(bold_result->typeface_data);

  // Verify the returned font is actually Bold — it must come from a different
  // file than Arial Regular ("ArialMT"). If the code were matching by family
  // name instead of full font name, it would match family "Arial" and could
  // return the regular face.
  mojom::MatchFamilyNameResultPtr regular_result;
  font_service_->MatchLocalFont("ArialMT", &regular_result);
  ASSERT_TRUE(regular_result);
  ASSERT_TRUE(bold_result->typeface_data->is_font_file());
  ASSERT_TRUE(regular_result->typeface_data->is_font_file());
  EXPECT_NE(bold_result->typeface_data->get_font_file()->id,
            regular_result->typeface_data->get_font_file()->id);
}

TEST_F(FontDataServiceImplUnitTest,
       MatchLocalFont_BoldByFullNameMatchesBoldByPostScript) {
  // Looking up "Arial Bold" (full name) and "Arial-BoldMT" (PS name) must
  // return the same font file — they refer to the same font face.
  mojom::MatchFamilyNameResultPtr full_name_result;
  font_service_->MatchLocalFont("Arial Bold", &full_name_result);
  ASSERT_TRUE(full_name_result);

  mojom::MatchFamilyNameResultPtr ps_result;
  font_service_->MatchLocalFont("Arial-BoldMT", &ps_result);
  ASSERT_TRUE(ps_result);

  ASSERT_TRUE(full_name_result->typeface_data->is_font_file());
  ASSERT_TRUE(ps_result->typeface_data->is_font_file());
  EXPECT_EQ(full_name_result->typeface_data->get_font_file()->id,
            ps_result->typeface_data->get_font_file()->id);
}

TEST_F(FontDataServiceImplUnitTest, MatchLocalFont_CaseInsensitive) {
  mojom::MatchFamilyNameResultPtr out_result;
  // DirectWrite font property matching is case-insensitive.
  font_service_->MatchLocalFont("arialmt", &out_result);
  ASSERT_TRUE(out_result);
  ExpectValidTypefaceData(out_result->typeface_data);
}

TEST_F(FontDataServiceImplUnitTest, MatchLocalFont_NotFound) {
  mojom::MatchFamilyNameResultPtr out_result;
  font_service_->MatchLocalFont("NonExistentFontXYZ_12345", &out_result);
  EXPECT_FALSE(out_result);
}

// Test fixture that sideloads the Ahem test font before constructing the
// FontDataServiceImpl, ensuring the DWrite font set includes sideloaded fonts.
class FontDataServiceImplSideloadTest : public testing::Test {
 protected:
  FontDataServiceImplSideloadTest() {
    // Sideload Ahem.ttf so it appears in the DWrite font set built by
    // DWriteLocalFontMatcher::GetLocalFontSet().
    base::FilePath base_path;
    base::PathService::Get(base::DIR_MODULE, &base_path);
    base::FilePath font_path =
        base_path.Append(FILE_PATH_LITERAL("test_fonts/Ahem.ttf"));
    gfx::win::SideLoadFontForTesting(font_path);

    receiver_.emplace(&impl_, font_service_.BindNewPipeAndPassReceiver());
  }

  base::test::SingleThreadTaskEnvironment environment_;
  mojo::Remote<mojom::FontDataService> font_service_;
  TestFontDataService impl_;
  std::optional<mojo::Receiver<mojom::FontDataService>> receiver_;
};

TEST_F(FontDataServiceImplSideloadTest, MatchLocalFont_SideloadedAhem) {
  mojom::MatchFamilyNameResultPtr out_result;
  // The Ahem font has family name, PostScript name, and full font name all
  // equal to "Ahem". It is not a system font — it must be found via the
  // sideloaded font set built in DWriteLocalFontMatcher::GetLocalFontSet().
  font_service_->MatchLocalFont("Ahem", &out_result);
  ASSERT_TRUE(out_result);
  ExpectValidTypefaceData(out_result->typeface_data);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

}  // namespace font_data_service
