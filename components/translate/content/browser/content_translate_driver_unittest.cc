// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/content_translate_driver.h"

#include <memory>

#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {
namespace {

// A simple test observer that tracks whether it was notified.
class TestTranslationObserver
    : public ContentTranslateDriver::TranslationObserver {
 public:
  TestTranslationObserver() = default;
  ~TestTranslationObserver() override = default;

  void OnIsPageTranslatedChanged(content::WebContents*) override {}
  void OnTranslateEnabledChanged(content::WebContents*) override {}
};

class ContentTranslateDriverTest : public content::RenderViewHostTestHarness {
 public:
  ContentTranslateDriverTest() = default;
  ~ContentTranslateDriverTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    driver_ = std::make_unique<ContentTranslateDriver>(
        *web_contents(), /*url_language_histogram=*/nullptr);
  }

  void TearDown() override {
    driver_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<ContentTranslateDriver> driver_;
};

// Test that adding and removing observers works correctly.
TEST_F(ContentTranslateDriverTest, AddRemoveObserver) {
  TestTranslationObserver observer;

  driver_->AddTranslationObserver(&observer);
  driver_->RemoveTranslationObserver(&observer);

  // Should not crash when driver is destroyed.
}

// Regression test for https://crbug.com/474819145
// Verifies that destroying ContentTranslateDriver with observers still
// registered does not crash. This can happen when WebContents is destroyed
// before Java-side cleanup callbacks have a chance to remove observers.
TEST_F(ContentTranslateDriverTest, DestroyWithObserverStillRegistered) {
  TestTranslationObserver observer;

  driver_->AddTranslationObserver(&observer);

  // Intentionally NOT removing the observer before destroying the driver.
  // This simulates the race condition where WebContents destruction happens
  // before the Java-side observer cleanup.
  driver_.reset();

  // If we get here without crashing, the test passes.
  // Previously, this would trigger a DUMP_WILL_BE_CHECK failure in
  // ObserverList's destructor because observers were still registered.
}

// Test with multiple observers not removed.
TEST_F(ContentTranslateDriverTest, DestroyWithMultipleObservers) {
  TestTranslationObserver observer1;
  TestTranslationObserver observer2;
  TestTranslationObserver observer3;

  driver_->AddTranslationObserver(&observer1);
  driver_->AddTranslationObserver(&observer2);
  driver_->AddTranslationObserver(&observer3);

  // Remove only one observer.
  driver_->RemoveTranslationObserver(&observer2);

  // Destroy with observer1 and observer3 still registered.
  driver_.reset();

  // Should not crash.
}

}  // namespace
}  // namespace translate
