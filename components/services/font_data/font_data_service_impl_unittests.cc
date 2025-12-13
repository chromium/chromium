// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font_data/font_data_service_impl.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "components/services/font_data/public/mojom/font_data_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  // For now, on Linux we always hit the memory region fallback, and therefore
  // also adds to the cache.
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
  // TODO(crbug.com/462090356): Find an available font in Linux with multiples
  // axes.
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

// The linux SkFontMgr doesn't support MatchFamilyStyleCharacter().
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

// The linux SkFontMgr doesn't support MatchFamilyStyleCharacter().
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

// The linux SkFontMgr doesn't support countFamilies().
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
  // For now, on Linux we always hit the memory region fallback.
  EXPECT_TRUE(out_result->typeface_data->is_region());
  EXPECT_TRUE(out_result->typeface_data->get_region().IsValid());
#endif
}

}  // namespace

}  // namespace font_data_service
