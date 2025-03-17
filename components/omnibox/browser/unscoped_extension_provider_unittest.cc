// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/unscoped_extension_provider.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/mock_unscoped_extension_provider_delegate.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using base::ASCIIToUTF16;
using ::testing::_;

class UnscopedExtensionProviderTest : public testing::Test {
 protected:
  UnscopedExtensionProviderTest() = default;
  void SetUp() override;
  void InitProvider(
      std::unique_ptr<MockUnscopedExtensionProviderDelegate> mock_delegate);

  base::test::TaskEnvironment task_environment_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<MockAutocompleteProviderClient> client_;
  // The provider is initialized by calling `InitProvider()` in
  // the tests after the expectations have been set on the delegate.
  scoped_refptr<UnscopedExtensionProvider> extension_provider_;
};

void UnscopedExtensionProviderTest::SetUp() {
  client_ = std::make_unique<MockAutocompleteProviderClient>();
  client_->set_template_url_service(
      search_engines_test_environment_.template_url_service());
}

void UnscopedExtensionProviderTest::InitProvider(
    std::unique_ptr<MockUnscopedExtensionProviderDelegate> mock_delegate) {
  client_->set_unscoped_extension_provider_delegate(std::move(mock_delegate));
  extension_provider_ = new UnscopedExtensionProvider(client_.get(), nullptr);
}

TEST_F(UnscopedExtensionProviderTest, RunsAndIncrementsRequestIdWithChanges) {
  std::unique_ptr<MockUnscopedExtensionProviderDelegate> mock_delegate =
      std::make_unique<MockUnscopedExtensionProviderDelegate>();
  client_->GetTemplateURLService()->AddToUnscopedModeExtensionIds("id");

  AutocompleteInput input(u"input", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  input.set_omit_asynchronous_matches(false);

  EXPECT_CALL(*mock_delegate, Stop);
  EXPECT_CALL(*mock_delegate, Start);

  InitProvider(std::move(mock_delegate));
  extension_provider_->Start(input, /*minimal_changes=*/false);
}

TEST_F(UnscopedExtensionProviderTest,
       DoesNotRunAndMaintainsRequestIdWithMinimalChanges) {
  std::unique_ptr<MockUnscopedExtensionProviderDelegate> mock_delegate =
      std::make_unique<MockUnscopedExtensionProviderDelegate>();
  client_->GetTemplateURLService()->AddToUnscopedModeExtensionIds("id");

  AutocompleteInput input(u"input", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);

  EXPECT_CALL(*mock_delegate, Stop);
  EXPECT_CALL(*mock_delegate, Start).Times(0);

  InitProvider(std::move(mock_delegate));
  extension_provider_->Start(input, /*minimal_changes=*/true);
}

TEST_F(UnscopedExtensionProviderTest,
       DoesNotRunWithChangesAndOmitAsyncMatches) {
  std::unique_ptr<MockUnscopedExtensionProviderDelegate> mock_delegate =
      std::make_unique<MockUnscopedExtensionProviderDelegate>();
  client_->GetTemplateURLService()->AddToUnscopedModeExtensionIds("id");

  AutocompleteInput input(u"input", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  input.set_omit_asynchronous_matches(true);

  EXPECT_CALL(*mock_delegate, Stop);
  EXPECT_CALL(*mock_delegate, Start).Times(0);

  InitProvider(std::move(mock_delegate));
  extension_provider_->Start(input, /*minimal_changes=*/false);
}

TEST_F(UnscopedExtensionProviderTest, DoesNotRunOnFocus) {
  std::unique_ptr<MockUnscopedExtensionProviderDelegate> mock_delegate =
      std::make_unique<MockUnscopedExtensionProviderDelegate>();
  client_->GetTemplateURLService()->AddToUnscopedModeExtensionIds("id");

  AutocompleteInput input(u"input", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);

  EXPECT_CALL(*mock_delegate, Stop);
  EXPECT_CALL(*mock_delegate, Start).Times(0);

  InitProvider(std::move(mock_delegate));
  extension_provider_->Start(input, /*minimal_changes=*/false);
}

TEST_F(UnscopedExtensionProviderTest, DoesNotRunWithNoUnscopedExtensions) {
  std::unique_ptr<MockUnscopedExtensionProviderDelegate> mock_delegate =
      std::make_unique<MockUnscopedExtensionProviderDelegate>();
  AutocompleteInput input(u"input", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_DEFAULT);

  EXPECT_CALL(*mock_delegate, Stop);
  EXPECT_CALL(*mock_delegate, Start).Times(0);

  InitProvider(std::move(mock_delegate));
  extension_provider_->Start(input, /*minimal_changes=*/true);
}
