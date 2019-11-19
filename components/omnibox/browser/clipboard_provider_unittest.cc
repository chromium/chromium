// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/clipboard_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/fake_clipboard_recent_content.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace {

const char kCurrentURL[] = "http://example.com/current";
const char kClipboardURL[] = "http://example.com/clipboard";
const char kClipboardText[] = "Search for me";
const char kClipboardTitleText[] = "\"Search for me\"";

}  // namespace

class ClipboardProviderTest : public testing::Test,
                              public AutocompleteProviderListener {
 public:
  ClipboardProviderTest()
      : client_(new MockAutocompleteProviderClient()),
        provider_(new ClipboardProvider(client_.get(),
                                        this,
                                        nullptr,
                                        &clipboard_content_)) {
    SetClipboardUrl(GURL(kClipboardURL));
  }

  ~ClipboardProviderTest() override {}

  void ClearClipboard() { clipboard_content_.SuppressClipboardContent(); }

  void SetClipboardUrl(const GURL& url) {
    clipboard_content_.SetClipboardURL(url, base::TimeDelta::FromMinutes(10));
  }

  void SetClipboardText(const base::string16& text) {
    clipboard_content_.SetClipboardText(text, base::TimeDelta::FromMinutes(10));
  }

  void SetClipboardImage(const gfx::Image& image) {
    clipboard_content_.SetClipboardImage(image,
                                         base::TimeDelta::FromMinutes(10));
  }

  bool IsClipboardEmpty() {
    return clipboard_content_.GetRecentURLFromClipboard() == base::nullopt &&
           clipboard_content_.GetRecentTextFromClipboard() == base::nullopt &&
           clipboard_content_.GetRecentImageFromClipboard() == base::nullopt;
  }

  AutocompleteInput CreateAutocompleteInput(bool from_omnibox_focus) {
    AutocompleteInput input(base::string16(), metrics::OmniboxEventProto::OTHER,
                            classifier_);
    input.set_current_url(GURL(kCurrentURL));
    input.set_from_omnibox_focus(from_omnibox_focus);
    return input;
  }

 protected:
  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches) override;

  TestSchemeClassifier classifier_;
  FakeClipboardRecentContent clipboard_content_;
  std::unique_ptr<MockAutocompleteProviderClient> client_;
  scoped_refptr<ClipboardProvider> provider_;
};

void ClipboardProviderTest::OnProviderUpdate(bool updated_matches) {
  // No action required.
}

TEST_F(ClipboardProviderTest, NotFromOmniboxFocus) {
  provider_->Start(CreateAutocompleteInput(false), false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(ClipboardProviderTest, EmptyClipboard) {
  ClearClipboard();
  provider_->Start(CreateAutocompleteInput(true), false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(ClipboardProviderTest, ClipboardIsCurrentURL) {
  SetClipboardUrl(GURL(kCurrentURL));
  provider_->Start(CreateAutocompleteInput(true), false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(ClipboardProviderTest, HasMultipleMatches) {
  provider_->Start(CreateAutocompleteInput(true), false);
  ASSERT_GE(provider_->matches().size(), 1U);
  EXPECT_EQ(GURL(kClipboardURL), provider_->matches().back().destination_url);
}

TEST_F(ClipboardProviderTest, MatchesText) {
  base::test::ScopedFeatureList feature_list;
  base::Feature textFeature = omnibox::kEnableClipboardProviderTextSuggestions;
  feature_list.InitAndEnableFeature(textFeature);
  auto template_url_service = std::make_unique<TemplateURLService>(
      /*initializers=*/nullptr, /*count=*/0);
  client_->set_template_url_service(std::move(template_url_service));
  SetClipboardText(base::UTF8ToUTF16(kClipboardText));
  provider_->Start(CreateAutocompleteInput(true), false);
  ASSERT_GE(provider_->matches().size(), 1U);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardTitleText),
            provider_->matches().back().contents);
}

TEST_F(ClipboardProviderTest, MatchesImage) {
  base::test::ScopedFeatureList feature_list;
  base::Feature imageFeature =
      omnibox::kEnableClipboardProviderImageSuggestions;
  feature_list.InitAndEnableFeature(imageFeature);
  TemplateURLService template_url_service(/*initializers=*/nullptr,
                                          /*count=*/0);
  base::TimeDelta clipboard_age = base::TimeDelta::FromSeconds(5);

  gfx::Image test_image = gfx::test::CreateImage(/*height=*/10, /*width=*/10);
  scoped_refptr<base::RefCountedMemory> image_bytes =
      provider_->EncodeClipboardImage(test_image);
  ASSERT_TRUE(image_bytes);
  provider_->ConstructImageMatchCallback(CreateAutocompleteInput(true),
                                         &template_url_service, clipboard_age,
                                         image_bytes);
  ASSERT_GE(provider_->matches().size(), 1U);
}

TEST_F(ClipboardProviderTest, DeleteMatch) {
  base::test::ScopedFeatureList feature_list;
  base::Feature textFeature = omnibox::kEnableClipboardProviderTextSuggestions;
  feature_list.InitAndEnableFeature(textFeature);
  auto template_url_service = std::make_unique<TemplateURLService>(
      /*initializers=*/nullptr, /*count=*/0);
  client_->set_template_url_service(std::move(template_url_service));
  SetClipboardText(base::UTF8ToUTF16(kClipboardText));
  provider_->Start(CreateAutocompleteInput(true), false);
  ASSERT_EQ(provider_->matches().size(), 1U);

  provider_->DeleteMatch(provider_->matches().back());
  ASSERT_EQ(provider_->matches().size(), 0U);
  ASSERT_TRUE(IsClipboardEmpty());
}
