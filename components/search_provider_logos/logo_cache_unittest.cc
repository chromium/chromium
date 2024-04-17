// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/logo_cache.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace search_provider_logos {

LogoMetadata GetExampleMetadata() {
  LogoMetadata metadata;
  metadata.source_url = GURL("http://google.com/mylogo");
  metadata.fingerprint = "LC4JVIZ5HVITQFKH0V70";
  EXPECT_TRUE(base::Time::FromString("98-05-05 05:05:06 GMT",
                                     &metadata.expiration_time));
  metadata.can_show_after_expiration = true;
  metadata.type = LogoType::ANIMATED;
  metadata.short_link = GURL("https://g.co/");
  metadata.on_click_url = GURL("https://www.google.com/search?q=chicken");
  metadata.animated_url = GURL("http://www.google.com/logos/doodle.png");
  metadata.dark_animated_url =
      GURL("http://www.google.com/logos/dark_doodle.png");
  metadata.alt_text = "A logo about chickens";
  metadata.mime_type = "image/jpeg";
  metadata.dark_mime_type = "image/jpeg";
  metadata.dark_background_color = "#ABC123";
  metadata.log_url = GURL("https://www.google.com/ddllog?a=b");
  metadata.dark_log_url = GURL("https://www.google.com/ddllog?a=dark");
  metadata.cta_log_url = GURL("https://www.google.com/ddllog?c=d");
  metadata.dark_cta_log_url = GURL("https://www.google.com/ddllog?c=dark");
  metadata.share_button_x = 200;
  metadata.share_button_y = 100;
  metadata.share_button_opacity = 0.5;
  metadata.share_button_icon = "test_img";
  metadata.share_button_bg = "#ff22ff";
  metadata.dark_share_button_x = 150;
  metadata.dark_share_button_y = 50;
  metadata.dark_share_button_opacity = 0.7;
  metadata.dark_share_button_icon = "dark_test_img";
  metadata.dark_share_button_bg = "#22ff22";
  metadata.width_px = 500;
  metadata.height_px = 200;
  metadata.dark_width_px = 600;
  metadata.dark_height_px = 230;
  return metadata;
}

LogoMetadata GetExampleMetadata2() {
  LogoMetadata metadata;
  metadata.source_url = GURL("https://www.example.com/thebestlogo?size=large");
  metadata.fingerprint = "bh4PLHdnEaQAPxNGRyMao1rOmVFTXuOdVhdrMmPV";
  EXPECT_TRUE(base::Time::FromString("17-04-04 07:10:58 GMT",
                                     &metadata.expiration_time));
  metadata.can_show_after_expiration = false;
  metadata.type = LogoType::INTERACTIVE;
  metadata.on_click_url = GURL("http://www.example.co.uk/welcome.php#top");
  metadata.full_page_url =
      GURL("http://www.example.co.uk/welcome.php#fpdoodle");
  metadata.alt_text = "This is a logo";
  metadata.mime_type = "image/png";
  return metadata;
}

base::RefCountedString* CreateExampleImage(size_t num_bytes) {
  base::RefCountedString* encoded_image_str = new base::RefCountedString();
  std::string& str = encoded_image_str->as_string();
  str.resize(num_bytes);
  for (size_t i = 0; i < num_bytes; ++i)
    str[i] = static_cast<char>(i);
  return encoded_image_str;
}

std::unique_ptr<EncodedLogo> GetExampleLogo() {
  auto logo = std::make_unique<EncodedLogo>();
  logo->encoded_image = CreateExampleImage(837);
  logo->dark_encoded_image = CreateExampleImage(738);
  logo->metadata = GetExampleMetadata();
  return logo;
}

std::unique_ptr<EncodedLogo> GetExampleLogo2() {
  auto logo = std::make_unique<EncodedLogo>();
  logo->encoded_image = CreateExampleImage(345);
  logo->dark_encoded_image = CreateExampleImage(543);
  logo->metadata = GetExampleMetadata2();
  return logo;
}

std::unique_ptr<EncodedLogo> GetExampleLogoWithoutImage() {
  auto logo = std::make_unique<EncodedLogo>();
  logo->encoded_image = nullptr;
  logo->dark_encoded_image = nullptr;
  logo->metadata = GetExampleMetadata2();
  return logo;
}

void ExpectMetadataEqual(const LogoMetadata& expected_metadata,
                         const LogoMetadata& actual_metadata) {
  EXPECT_EQ(expected_metadata.source_url, actual_metadata.source_url);
  EXPECT_EQ(expected_metadata.fingerprint, actual_metadata.fingerprint);
  EXPECT_EQ(expected_metadata.can_show_after_expiration,
            actual_metadata.can_show_after_expiration);
  EXPECT_EQ(expected_metadata.expiration_time, actual_metadata.expiration_time);
  EXPECT_EQ(expected_metadata.type, actual_metadata.type);
  EXPECT_EQ(expected_metadata.on_click_url, actual_metadata.on_click_url);
  EXPECT_EQ(expected_metadata.full_page_url, actual_metadata.full_page_url);
  EXPECT_EQ(expected_metadata.animated_url, actual_metadata.animated_url);
  EXPECT_EQ(expected_metadata.alt_text, actual_metadata.alt_text);
  EXPECT_EQ(expected_metadata.mime_type, actual_metadata.mime_type);
  EXPECT_EQ(expected_metadata.log_url, actual_metadata.log_url);
  EXPECT_EQ(expected_metadata.dark_log_url, actual_metadata.dark_log_url);
  EXPECT_EQ(expected_metadata.cta_log_url, actual_metadata.cta_log_url);
  EXPECT_EQ(expected_metadata.dark_cta_log_url,
            actual_metadata.dark_cta_log_url);
  EXPECT_EQ(expected_metadata.short_link, actual_metadata.short_link);
  EXPECT_EQ(expected_metadata.share_button_x, actual_metadata.share_button_x);
  EXPECT_EQ(expected_metadata.share_button_y, actual_metadata.share_button_y);
  EXPECT_EQ(expected_metadata.share_button_opacity,
            actual_metadata.share_button_opacity);
  EXPECT_EQ(expected_metadata.share_button_icon,
            actual_metadata.share_button_icon);
  EXPECT_EQ(expected_metadata.share_button_bg, actual_metadata.share_button_bg);
  EXPECT_EQ(expected_metadata.dark_share_button_x,
            actual_metadata.dark_share_button_x);
  EXPECT_EQ(expected_metadata.dark_share_button_y,
            actual_metadata.dark_share_button_y);
  EXPECT_EQ(expected_metadata.dark_share_button_opacity,
            actual_metadata.dark_share_button_opacity);
  EXPECT_EQ(expected_metadata.dark_share_button_icon,
            actual_metadata.dark_share_button_icon);
  EXPECT_EQ(expected_metadata.dark_share_button_bg,
            actual_metadata.dark_share_button_bg);
  EXPECT_EQ(expected_metadata.width_px, actual_metadata.width_px);
  EXPECT_EQ(expected_metadata.height_px, actual_metadata.height_px);
  EXPECT_EQ(expected_metadata.dark_width_px, actual_metadata.dark_width_px);
  EXPECT_EQ(expected_metadata.dark_height_px, actual_metadata.dark_height_px);
  EXPECT_EQ(expected_metadata.iframe_width_px, actual_metadata.iframe_width_px);
  EXPECT_EQ(expected_metadata.iframe_height_px,
            actual_metadata.iframe_height_px);
  EXPECT_EQ(expected_metadata.dark_background_color,
            actual_metadata.dark_background_color);
}

void ExpectLogosEqual(const EncodedLogo& expected_logo,
                      const EncodedLogo& actual_logo) {
  ASSERT_TRUE(expected_logo.encoded_image.get());
  ASSERT_TRUE(actual_logo.encoded_image.get());
  EXPECT_TRUE(expected_logo.encoded_image->Equals(actual_logo.encoded_image));
  ASSERT_TRUE(expected_logo.dark_encoded_image.get());
  ASSERT_TRUE(actual_logo.dark_encoded_image.get());
  EXPECT_TRUE(
      expected_logo.dark_encoded_image->Equals(actual_logo.dark_encoded_image));
  ExpectMetadataEqual(expected_logo.metadata, actual_logo.metadata);
}

// Removes 1 byte from the end of the file at |path|.
void ShortenFile(base::FilePath path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  int64_t file_length = file.GetLength();
  ASSERT_GT(file_length, 0);
  file.SetLength(file_length - 1);
}

class LogoCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(cache_parent_dir_.CreateUniqueTempDir());
    InitCache();
  }

  void InitCache() {
    cache_ = std::make_unique<LogoCache>(
        cache_parent_dir_.GetPath().Append(FILE_PATH_LITERAL("cache")));
  }

  void ExpectMetadata(const LogoMetadata* expected_metadata) {
    const LogoMetadata* retrieved_metadata = cache_->GetCachedLogoMetadata();
    if (expected_metadata) {
      ASSERT_TRUE(retrieved_metadata);
      ExpectMetadataEqual(*expected_metadata, *retrieved_metadata);
    } else {
      ASSERT_FALSE(retrieved_metadata);
    }
  }

  void ExpectLogo(const EncodedLogo* expected_logo) {
    std::unique_ptr<EncodedLogo> retrieved_logo(cache_->GetCachedLogo());
    if (expected_logo) {
      ASSERT_TRUE(retrieved_logo.get());
      ExpectLogosEqual(*expected_logo, *retrieved_logo);
    } else {
      ASSERT_FALSE(retrieved_logo);
    }
  }

  void ExpectLogoWithoutImage(const EncodedLogo* expected_logo) {
    std::unique_ptr<EncodedLogo> retrieved_logo(cache_->GetCachedLogo());
    ASSERT_TRUE(retrieved_logo.get());
    ASSERT_FALSE(retrieved_logo->encoded_image.get());
    ASSERT_FALSE(expected_logo->encoded_image.get());
    ExpectMetadataEqual(expected_logo->metadata, retrieved_logo->metadata);
  }

  // Deletes the existing LogoCache and creates a new one. This clears any
  // logo or metadata cached in memory to simulate restarting Chrome.
  void SimulateRestart() {
    InitCache();
  }

  std::unique_ptr<LogoCache> cache_;
  base::ScopedTempDir cache_parent_dir_;
};

// Tests -----------------------------------------------------------------------

TEST(LogoCacheSerializationTest, SerializeMetadata) {
  LogoMetadata metadata = GetExampleMetadata();
  std::string metadata_str;
  int logo_num_bytes = 33;
  int dark_logo_num_bytes = 44;
  LogoCache::LogoMetadataToString(metadata, logo_num_bytes, dark_logo_num_bytes,
                                  &metadata_str);
  std::unique_ptr<LogoMetadata> metadata2 = LogoCache::LogoMetadataFromString(
      metadata_str, &logo_num_bytes, &dark_logo_num_bytes);
  ASSERT_TRUE(metadata2);
  ExpectMetadataEqual(metadata, *metadata2);
}

TEST(LogoCacheSerializationTest, DeserializeCorruptMetadata) {
  int logo_num_bytes = 33;
  int dark_logo_num_bytes = 44;
  std::unique_ptr<LogoMetadata> metadata = LogoCache::LogoMetadataFromString(
      "", &logo_num_bytes, &dark_logo_num_bytes);
  ASSERT_FALSE(metadata);

  LogoMetadata example_metadata = GetExampleMetadata2();
  std::string corrupt_str;
  LogoCache::LogoMetadataToString(example_metadata, logo_num_bytes,
                                  dark_logo_num_bytes, &corrupt_str);
  corrupt_str.append("@");
  metadata = LogoCache::LogoMetadataFromString(corrupt_str, &logo_num_bytes,
                                               &dark_logo_num_bytes);
  ASSERT_FALSE(metadata);
}

TEST_F(LogoCacheTest, StoreAndRetrieveMetadata) {
  // Expect no metadata at first.
  ExpectMetadata(nullptr);

  // Set initial metadata.
  std::unique_ptr<EncodedLogo> logo = GetExampleLogo();
  LogoMetadata& metadata = logo->metadata;
  cache_->SetCachedLogo(logo.get());
  ExpectMetadata(&metadata);

  // Update metadata.
  metadata.on_click_url = GURL("http://anotherwebsite.com");
  cache_->UpdateCachedLogoMetadata(metadata);
  ExpectMetadata(&metadata);

  // Read metadata back from disk.
  SimulateRestart();
  ExpectMetadata(&metadata);

  // Ensure metadata is cached in memory.
  base::DeleteFile(cache_->GetMetadataPath());
  ExpectMetadata(&metadata);
}

TEST_F(LogoCacheTest, StoreAndRetrieveLogo) {
  // Expect no metadata at first.
  ExpectLogo(nullptr);

  // Set initial logo.
  std::unique_ptr<EncodedLogo> logo = GetExampleLogo();
  cache_->SetCachedLogo(logo.get());
  ExpectLogo(logo.get());

  // Update logo to null.
  cache_->SetCachedLogo(nullptr);
  ExpectLogo(nullptr);

  // Read logo back from disk.
  SimulateRestart();
  ExpectLogo(nullptr);

  // Update logo.
  logo = GetExampleLogo2();
  cache_->SetCachedLogo(logo.get());
  ExpectLogo(logo.get());

  // Read logo back from disk.
  SimulateRestart();
  ExpectLogo(logo.get());
}

TEST_F(LogoCacheTest, StoreAndRetrieveLogoWithoutImage) {
  // Expect no metadata at first.
  ExpectLogo(nullptr);

  // Set initial logo.
  std::unique_ptr<EncodedLogo> logo = GetExampleLogoWithoutImage();
  cache_->SetCachedLogo(logo.get());
  ExpectLogoWithoutImage(logo.get());

  // Update logo to null.
  cache_->SetCachedLogo(nullptr);
  ExpectLogo(nullptr);

  // Read logo back from disk.
  SimulateRestart();
  ExpectLogo(nullptr);

  // Update logo.
  logo = GetExampleLogoWithoutImage();
  cache_->SetCachedLogo(logo.get());
  ExpectLogoWithoutImage(logo.get());

  // Read logo back from disk.
  SimulateRestart();
  ExpectLogoWithoutImage(logo.get());
}

TEST_F(LogoCacheTest, RetrieveCorruptMetadata) {
  // Set initial logo.
  std::unique_ptr<EncodedLogo> logo = GetExampleLogo2();
  cache_->SetCachedLogo(logo.get());
  ExpectLogo(logo.get());

  // Corrupt metadata and expect null for both logo and metadata.
  SimulateRestart();
  ShortenFile(cache_->GetMetadataPath());
  ExpectMetadata(nullptr);
  ExpectLogo(nullptr);

  // Ensure corrupt cache files are deleted.
  EXPECT_FALSE(base::PathExists(cache_->GetMetadataPath()));
  EXPECT_FALSE(base::PathExists(cache_->GetLogoPath()));
}

TEST_F(LogoCacheTest, RetrieveCorruptLogo) {
  // Set initial logo.
  std::unique_ptr<EncodedLogo> logo = GetExampleLogo();
  cache_->SetCachedLogo(logo.get());
  ExpectLogo(logo.get());

  // Corrupt logo and expect nullptr.
  SimulateRestart();
  ShortenFile(cache_->GetLogoPath());
  ExpectLogo(nullptr);

  // Once the logo is noticed to be null, the metadata should also be cleared.
  ExpectMetadata(nullptr);

  // Ensure corrupt cache files are deleted.
  EXPECT_FALSE(base::PathExists(cache_->GetMetadataPath()));
  EXPECT_FALSE(base::PathExists(cache_->GetLogoPath()));
}

}  // namespace search_provider_logos
