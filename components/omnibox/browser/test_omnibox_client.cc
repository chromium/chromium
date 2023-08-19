// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/test_omnibox_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

TestOmniboxClient::TestOmniboxClient()
    : session_id_(SessionID::FromSerializedValue(1)),
      autocomplete_classifier_(
          std::make_unique<AutocompleteController>(
              CreateAutocompleteProviderClient(),
              AutocompleteClassifier::DefaultOmniboxProviders()),
          std::make_unique<TestSchemeClassifier>()) {}

TestOmniboxClient::~TestOmniboxClient() {
  template_url_service_ = nullptr;
  autocomplete_classifier_.Shutdown();
}

std::unique_ptr<AutocompleteProviderClient>
TestOmniboxClient::CreateAutocompleteProviderClient() {
  auto provider_client = std::make_unique<MockAutocompleteProviderClient>();
  EXPECT_CALL(*provider_client, GetBuiltinURLs())
      .WillRepeatedly(testing::Return(std::vector<std::u16string>()));
  EXPECT_CALL(*provider_client, GetSchemeClassifier())
      .WillRepeatedly(testing::ReturnRef(scheme_classifier_));

  auto template_url_service = std::make_unique<TemplateURLService>(
      nullptr /* PrefService */, std::make_unique<SearchTermsData>(),
      nullptr /* KeywordWebDataService */,
      std::unique_ptr<TemplateURLServiceClient>(), base::RepeatingClosure());

  // Save a reference to the created TemplateURLService for test use.
  template_url_service_ = template_url_service.get();

  provider_client->set_template_url_service(std::move(template_url_service));

  return std::move(provider_client);
}

bool TestOmniboxClient::IsPasteAndGoEnabled() const {
  return true;
}

SessionID TestOmniboxClient::GetSessionID() const {
  return session_id_;
}

AutocompleteControllerEmitter*
TestOmniboxClient::GetAutocompleteControllerEmitter() {
  return nullptr;
}

TemplateURLService* TestOmniboxClient::GetTemplateURLService() {
  DCHECK(template_url_service_);
  return template_url_service_;
}

const AutocompleteSchemeClassifier& TestOmniboxClient::GetSchemeClassifier()
    const {
  return scheme_classifier_;
}

AutocompleteClassifier* TestOmniboxClient::GetAutocompleteClassifier() {
  return &autocomplete_classifier_;
}

bool TestOmniboxClient::ShouldDefaultTypedNavigationsToHttps() const {
  return false;
}

int TestOmniboxClient::GetHttpsPortForTesting() const {
  return 0;
}

bool TestOmniboxClient::IsUsingFakeHttpsForHttpsUpgradeTesting() const {
  return false;
}

gfx::Image TestOmniboxClient::GetSizedIcon(
    const gfx::VectorIcon& vector_icon_type,
    SkColor vector_icon_color) const {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  return gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}
