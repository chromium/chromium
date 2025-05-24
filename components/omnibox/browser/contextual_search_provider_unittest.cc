// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/contextual_search_provider.h"

#include <memory>
#include <ranges>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "url/gurl.h"

class ContextualSearchProviderTest : public testing::Test,
                                     public AutocompleteProviderListener {
 public:
  ContextualSearchProviderTest() = default;
  ContextualSearchProviderTest(const ContextualSearchProviderTest&) = delete;
  ContextualSearchProviderTest& operator=(const ContextualSearchProviderTest&) =
      delete;

  void SetUp() override {
    client_ = std::make_unique<MockAutocompleteProviderClient>();
    provider_ = new ContextualSearchProvider(client_.get(), this);
  }

 protected:
  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override {}

  std::unique_ptr<MockAutocompleteProviderClient> client_;
  scoped_refptr<ContextualSearchProvider> provider_;
};

TEST_F(ContextualSearchProviderTest, LensAdActionConditions) {
  auto has_actions = [this] {
    return std::ranges::any_of(provider_->matches(), [](const auto& match) {
      return !!match.takeover_action;
    });
  };
  EXPECT_CALL(*client_, IsLensEnabled()).WillRepeatedly(testing::Return(true));

  {
    AutocompleteInput input(u"nonempty input text",
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://example.com"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    EXPECT_FALSE(has_actions());
  }
  {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://example.com"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    EXPECT_TRUE(has_actions());
  }
  {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("chrome://flags"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    EXPECT_FALSE(has_actions());
  }
  {
    // Include action for local files.
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("file:///home/me/personal/local/file.pdf"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    EXPECT_TRUE(has_actions());
  }
  {
    // Exclude action from other local schemes.
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("chrome://flags"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    EXPECT_FALSE(has_actions());
  }
  {
    // Lens action missing if Lens is disabled.
    EXPECT_CALL(*client_, IsLensEnabled()).WillOnce(testing::Return(false));
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://example.com"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    EXPECT_FALSE(has_actions());
  }
  {
    // When backspacing to empty input, action should not be shown.
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://example.com"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    EXPECT_FALSE(has_actions());
  }
}

TEST_F(ContextualSearchProviderTest, LensAdActionFillsEditAndElidesWwwOnly) {
  EXPECT_CALL(*client_, IsLensEnabled()).WillRepeatedly(testing::Return(true));
  {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://something.example.com"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    const AutocompleteMatch& match = provider_->matches()[0];
    EXPECT_FALSE(match.fill_into_edit.empty());
    EXPECT_EQ(match.contents, u"something.example.com");
  }
  {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("https://www.example.com"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    const AutocompleteMatch& match = provider_->matches()[0];
    EXPECT_FALSE(match.fill_into_edit.empty());
    EXPECT_EQ(match.contents, u"example.com");
  }
  {
    AutocompleteInput input(u"", metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL("file:///home/personal/file.pdf"));
    input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
    provider_->Start(input, false);
    EXPECT_TRUE(provider_->done());
    const AutocompleteMatch& match = provider_->matches()[0];
    EXPECT_FALSE(match.fill_into_edit.empty());
    EXPECT_EQ(match.contents, u"");
  }
}
