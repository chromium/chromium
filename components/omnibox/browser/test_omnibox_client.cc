// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/test_omnibox_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_config.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/mock_unscoped_extension_provider_delegate.h"
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
              AutocompleteControllerConfig{
                  .provider_types =
                      AutocompleteClassifier::DefaultOmniboxProviders()}),
          std::make_unique<TestSchemeClassifier>()),
      last_log_disposition_(WindowOpenDisposition::UNKNOWN) {}

TestOmniboxClient::~TestOmniboxClient() {
  autocomplete_classifier_.Shutdown();
}

std::unique_ptr<AutocompleteProviderClient>
TestOmniboxClient::CreateAutocompleteProviderClient() {
  auto provider_client = std::make_unique<FakeAutocompleteProviderClient>();
  EXPECT_CALL(*provider_client, GetBuiltinURLs())
      .WillRepeatedly(testing::Return(std::vector<std::u16string>()));
  EXPECT_CALL(*provider_client, GetSchemeClassifier())
      .WillRepeatedly(testing::ReturnRef(scheme_classifier_));
  EXPECT_CALL(*provider_client, GetApplicationLocale())
      .WillRepeatedly(testing::Return("en-US"));

  // The `UnscopedExtensionProviderDelegate` should be set. It will be called
  // when `AutocompleteController::Stop()` is called on destruction.
  std::unique_ptr<MockUnscopedExtensionProviderDelegate> mock_delegate =
      std::make_unique<MockUnscopedExtensionProviderDelegate>();
  provider_client->set_unscoped_extension_provider_delegate(
      std::move(mock_delegate));
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
  CHECK(search_engines_test_environment_.template_url_service());
  return search_engines_test_environment_.template_url_service();
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

gfx::Image TestOmniboxClient::GetSizedIcon(const SkBitmap* bitmap) const {
  return gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
}

gfx::Image TestOmniboxClient::GetSizedIcon(
    const gfx::VectorIcon& vector_icon_type,
    SkColor vector_icon_color) const {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  return gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}

gfx::Image TestOmniboxClient::GetSizedIcon(const gfx::Image& icon) const {
  if (icon.IsEmpty()) {
    return gfx::Image();
  }
  return gfx::Image(*icon.ToImageSkia());
}

std::u16string TestOmniboxClient::GetFormattedFullURL() const {
  return location_bar_model_.GetFormattedFullURL();
}

std::u16string TestOmniboxClient::GetURLForDisplay() const {
  return location_bar_model_.GetURLForDisplay();
}

GURL TestOmniboxClient::GetNavigationEntryURL() const {
  return location_bar_model_.GetURL();
}

metrics::OmniboxEventProto::PageClassification
TestOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return location_bar_model_.GetPageClassification(is_prefetch);
}

security_state::SecurityLevel TestOmniboxClient::GetSecurityLevel() const {
  return location_bar_model_.GetSecurityLevel();
}

net::CertStatus TestOmniboxClient::GetCertStatus() const {
  return location_bar_model_.GetCertStatus();
}

const gfx::VectorIcon& TestOmniboxClient::GetVectorIcon() const {
  return location_bar_model_.GetVectorIcon();
}

void TestOmniboxClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  last_log_disposition_ = log->disposition;
}

base::WeakPtr<OmniboxClient> TestOmniboxClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
