// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_icon_image_loader.h"

#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/chromeos/launcher_search_provider/error_reporter.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

using chromeos::launcher_search_provider::ErrorReporter;

namespace app_list {

namespace {

const char kTestExtensionId[] = "foo";
const char kTestCustomIconURL[] = "chrome-extension://foo/bar";

// Generates an image source which is filled with |fill_color|.
class FillColorImageSource : public gfx::CanvasImageSource {
 public:
  FillColorImageSource(const gfx::Size& image_size, const SkColor fill_color)
      : CanvasImageSource(image_size), fill_color_(fill_color) {}

  void Draw(gfx::Canvas* canvas) override {
    canvas->FillRect(gfx::Rect(size()), fill_color_);
  }

 private:
  const SkColor fill_color_;

  DISALLOW_COPY_AND_ASSIGN(FillColorImageSource);
};

// Test implementation of LauncherSearchIconImageLoader.
class LauncherSearchIconImageLoaderTestImpl
    : public LauncherSearchIconImageLoader {
 public:
  // Use base class constructor.
  using LauncherSearchIconImageLoader::LauncherSearchIconImageLoader;

  const gfx::ImageSkia& LoadExtensionIcon() override {
    // Returns 32x32 black image.
    extension_icon_ = gfx::ImageSkia(
        std::make_unique<FillColorImageSource>(icon_size_, SK_ColorBLACK),
        icon_size_);
    return extension_icon_;
  }

  // Calls OnExtensionIconImageChnaged callback with |extension_icon|.
  void LoadExtensionIconAsync(const gfx::ImageSkia& image) {
    OnExtensionIconChanged(image);
  }

  void LoadIconResourceFromExtension() override {
    // For success case, returns 32x32 blue image.
    is_load_extension_icon_resource_called_ = true;
  }

  bool IsLoadExtensionIconResourceCalled() const {
    return is_load_extension_icon_resource_called_;
  }

  // Calls OnCustomIconLoaded callback with |custom_icon|. Sets an empty image
  // for simulating a failure case.
  void CallOnCustomIconLoaded(gfx::ImageSkia custom_icon) {
    OnCustomIconLoaded(custom_icon);
  }

 private:
  // Ref counted class.
  ~LauncherSearchIconImageLoaderTestImpl() override = default;

  gfx::ImageSkia extension_icon_;
  bool is_load_extension_icon_resource_called_ = false;
};

// A fake error reporter to test error message.
class FakeErrorReporter : public ErrorReporter {
 public:
  FakeErrorReporter() : ErrorReporter(nullptr) {}
  ~FakeErrorReporter() override {}

  void Warn(const std::string& message) override { last_message_ = message; }

  const std::string& GetLastWarningMessage() const { return last_message_; }

 private:
  std::string last_message_;

  DISALLOW_COPY_AND_ASSIGN(FakeErrorReporter);
};

// Creates a test extension with |extension_id|.
scoped_refptr<extensions::Extension> CreateTestExtension(
    const std::string& extension_id) {
  base::DictionaryValue manifest;
  std::string error;
  manifest.SetKey(extensions::manifest_keys::kVersion, base::Value("1"));
  manifest.SetKey(extensions::manifest_keys::kManifestVersion, base::Value(2));
  manifest.SetKey(extensions::manifest_keys::kName,
                  base::Value("TestExtension"));
  return extensions::Extension::Create(
      base::FilePath(), extensions::Manifest::UNPACKED, manifest,
      extensions::Extension::NO_FLAGS, extension_id, &error);
}

// Returns true if icon image of |result_image| equals to |expected_image|.
bool IsEqual(const gfx::ImageSkia& expected_image,
             const gfx::ImageSkia& result_image) {
  return gfx::test::AreBitmapsEqual(
      expected_image.GetRepresentation(1.0).GetBitmap(),
      result_image.GetRepresentation(1.0).GetBitmap());
}

}  // namespace

class LauncherSearchIconImageLoaderTest : public testing::Test {
 protected:
  void SetUp() override { extension_ = CreateTestExtension(kTestExtensionId); }

  std::unique_ptr<FakeErrorReporter> GetFakeErrorReporter() {
    return std::make_unique<FakeErrorReporter>();
  }

  scoped_refptr<extensions::Extension> extension_;
};

TEST_F(LauncherSearchIconImageLoaderTest, WithoutCustomIconSuccessCase) {
  GURL icon_url;  // No custom icon.
  auto impl = base::MakeRefCounted<LauncherSearchIconImageLoaderTestImpl>(
      icon_url, nullptr, nullptr, 32, GetFakeErrorReporter());
  impl->LoadResources();

  // Assert that extension icon image is set to icon image and badge icon image
  // is null.
  gfx::Size icon_size(32, 32);
  gfx::ImageSkia expected_image(
      std::make_unique<FillColorImageSource>(icon_size, SK_ColorBLACK),
      icon_size);
  ASSERT_TRUE(IsEqual(expected_image, impl->GetIconImage()));

  ASSERT_TRUE(impl->GetBadgeIconImage().isNull());
}

TEST_F(LauncherSearchIconImageLoaderTest, ExtensionIconAsyncLoadSuccessCase) {
  GURL icon_url;  // No custom icon.
  auto impl = base::MakeRefCounted<LauncherSearchIconImageLoaderTestImpl>(
      icon_url, nullptr, nullptr, 32, GetFakeErrorReporter());
  impl->LoadResources();

  // Extension icon is loaded as async.
  gfx::Size icon_size(32, 32);
  gfx::ImageSkia extension_icon(
      std::make_unique<FillColorImageSource>(icon_size, SK_ColorGREEN),
      icon_size);
  impl->LoadExtensionIconAsync(extension_icon);

  // Assert that the asynchronously loaded image is set to icon image and badge
  // icon image is null.
  gfx::ImageSkia expected_image(
      std::make_unique<FillColorImageSource>(icon_size, SK_ColorGREEN),
      icon_size);
  ASSERT_TRUE(IsEqual(expected_image, impl->GetIconImage()));

  ASSERT_TRUE(impl->GetBadgeIconImage().isNull());
}

TEST_F(LauncherSearchIconImageLoaderTest, WithCustomIconSuccessCase) {
  GURL icon_url(kTestCustomIconURL);
  auto impl = base::MakeRefCounted<LauncherSearchIconImageLoaderTestImpl>(
      icon_url, nullptr, extension_.get(), 32, GetFakeErrorReporter());
  ASSERT_FALSE(impl->IsLoadExtensionIconResourceCalled());
  impl->LoadResources();

  // Assert that LoadExtensionIconResource is called.
  ASSERT_TRUE(impl->IsLoadExtensionIconResourceCalled());

  // Load custom icon as async.
  gfx::Size icon_size(32, 32);
  gfx::ImageSkia custom_icon(
      std::make_unique<FillColorImageSource>(icon_size, SK_ColorGREEN),
      icon_size);
  impl->CallOnCustomIconLoaded(custom_icon);

  // Assert that custom icon image is set to icon image and extension icon image
  // is set to badge icon image.
  gfx::ImageSkia expected_image(
      std::make_unique<FillColorImageSource>(icon_size, SK_ColorGREEN),
      icon_size);
  ASSERT_TRUE(IsEqual(expected_image, impl->GetIconImage()));

  gfx::ImageSkia expected_badge_icon_image(
      std::make_unique<FillColorImageSource>(icon_size, SK_ColorBLACK),
      icon_size);
  ASSERT_TRUE(IsEqual(expected_badge_icon_image, impl->GetBadgeIconImage()));
}

TEST_F(LauncherSearchIconImageLoaderTest, InvalidCustomIconUrl) {
  // Use a really long URL (for testing the string truncation).
  // The URL is from the wrong extension (foo2), so should be rejected.
  std::string invalid_url =
      "chrome-extension://foo2/bar/"
      "901234567890123456789012345678901234567890123456789012345678901234567890"
      "1";
  ASSERT_EQ(101U, invalid_url.size());

  std::unique_ptr<FakeErrorReporter> fake_error_reporter =
      GetFakeErrorReporter();
  FakeErrorReporter* fake_error_reporter_ptr = fake_error_reporter.get();
  GURL icon_url(invalid_url);
  auto impl = base::MakeRefCounted<LauncherSearchIconImageLoaderTestImpl>(
      icon_url, nullptr, extension_.get(), 32, std::move(fake_error_reporter));
  impl->LoadResources();

  // Warning message should be provided.
  ASSERT_EQ(
      "[chrome.launcherSearchProvider.setSearchResults] Invalid icon URL: "
      "chrome-extension://foo2/bar/"
      "901234567890123456789012345678901234567890123456789012345678901234567..."
      ". Must have a valid URL within chrome-extension://foo.",
      fake_error_reporter_ptr->GetLastWarningMessage());

  // LoadExtensionIconResource should not be called.
  ASSERT_FALSE(impl->IsLoadExtensionIconResourceCalled());
}

TEST_F(LauncherSearchIconImageLoaderTest, FailedToLoadCustomIcon) {
  std::unique_ptr<FakeErrorReporter> fake_error_reporter =
      GetFakeErrorReporter();
  FakeErrorReporter* fake_error_reporter_ptr = fake_error_reporter.get();
  GURL icon_url(kTestCustomIconURL);
  auto impl = base::MakeRefCounted<LauncherSearchIconImageLoaderTestImpl>(
      icon_url, nullptr, extension_.get(), 32, std::move(fake_error_reporter));
  impl->LoadResources();
  ASSERT_TRUE(impl->IsLoadExtensionIconResourceCalled());

  // Fails to load custom icon by passing an empty image.
  gfx::ImageSkia custom_icon;
  impl->CallOnCustomIconLoaded(custom_icon);

  // Warning message should be shown.
  ASSERT_EQ(
      "[chrome.launcherSearchProvider.setSearchResults] Failed to load icon "
      "URL: chrome-extension://foo/bar",
      fake_error_reporter_ptr->GetLastWarningMessage());

  // Assert that extension icon image is set to icon image and badge icon image
  // is null.
  gfx::Size icon_size(32, 32);
  gfx::ImageSkia expected_image(
      std::make_unique<FillColorImageSource>(icon_size, SK_ColorBLACK),
      icon_size);
  ASSERT_TRUE(IsEqual(expected_image, impl->GetIconImage()));

  ASSERT_TRUE(impl->GetBadgeIconImage().isNull());
}

}  // namespace app_list
