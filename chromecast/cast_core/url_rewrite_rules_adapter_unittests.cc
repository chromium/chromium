// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/url_rewrite_rules_adapter.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

using ::testing::_;

class MockIdentificationSettingsManager final
    : public mojom::IdentificationSettingsManager {
 public:
  MockIdentificationSettingsManager() = default;
  ~MockIdentificationSettingsManager() override = default;

  MOCK_METHOD(void,
              SetSubstitutableParameters,
              (std::vector<mojom::SubstitutableParameterPtr>),
              (override));
  MOCK_METHOD(void,
              SetClientAuth,
              (mojo::PendingRemote<mojom::ClientAuthDelegate>),
              (override));
  MOCK_METHOD(void, UpdateAppSettings, (mojom::AppSettingsPtr), (override));
  MOCK_METHOD(void,
              UpdateDeviceSettings,
              (mojom::DeviceSettingsPtr),
              (override));
  MOCK_METHOD(void,
              UpdateSubstitutableParamValues,
              (std::vector<mojom::IndexValuePairPtr>),
              (override));
  MOCK_METHOD(void, UpdateBackgroundMode, (bool), (override));

  void Bind(
      mojo::PendingAssociatedReceiver<mojom::IdentificationSettingsManager>
          pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  mojo::PendingAssociatedRemote<mojom::IdentificationSettingsManager> Bind() {
    return receiver_.BindNewEndpointAndPassDedicatedRemote();
  }

 private:
  mojo::AssociatedReceiver<mojom::IdentificationSettingsManager> receiver_{
      this};
};

class UrlRewriteRulesAdapterTest : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockIdentificationSettingsManager settings_manager_;
  std::unique_ptr<UrlRewriteRulesAdapter> url_rewrite_adapter_;
};

}  // namespace

TEST_F(UrlRewriteRulesAdapterTest, StaticHeaders) {
  cast::v2::UrlRequestRewriteRules first_rules;
  {
    auto* rule = first_rules.add_rules();
    rule->set_action(
        cast::v2::UrlRequestRewriteRule_UrlRequestAction_ACTION_UNSPECIFIED);
    auto* add_headers = rule->add_rewrites()->mutable_add_headers();
    cast::v2::UrlHeader fake_header;
    fake_header.set_name("fake_header_name");
    fake_header.set_value("some-value");
    *add_headers->add_headers() = std::move(fake_header);
    fake_header.set_name("other_header");
    fake_header.set_value("other-value");
    *add_headers->add_headers() = std::move(fake_header);
  }

  mojo::AssociatedRemote<mojom::IdentificationSettingsManager> remote(
      settings_manager_.Bind());
  url_rewrite_adapter_ = std::make_unique<UrlRewriteRulesAdapter>(first_rules);
  EXPECT_CALL(settings_manager_, UpdateDeviceSettings(_))
      .WillOnce([](mojom::DeviceSettingsPtr device_settings) {
        EXPECT_EQ(device_settings->static_headers.size(), 2u);
        EXPECT_EQ(device_settings->static_headers["fake_header_name"],
                  "some-value");
        EXPECT_EQ(device_settings->static_headers["other_header"],
                  "other-value");
        EXPECT_EQ(device_settings->url_replacements.size(), 0u);
      });
  url_rewrite_adapter_->AddRenderFrame(std::move(remote));

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(UrlRewriteRulesAdapterTest, UrlReplacements) {
  cast::v2::UrlRequestRewriteRules first_rules;
  {
    auto* rule = first_rules.add_rules();
    rule->set_action(
        cast::v2::UrlRequestRewriteRule_UrlRequestAction_ACTION_UNSPECIFIED);
    auto* replace_url = rule->add_rewrites()->mutable_replace_url();
    replace_url->set_url_ends_with("www.example.com");
    replace_url->set_new_url("sub2.other-example.com");
  }

  mojo::AssociatedRemote<mojom::IdentificationSettingsManager> remote(
      settings_manager_.Bind());
  url_rewrite_adapter_ = std::make_unique<UrlRewriteRulesAdapter>(first_rules);
  EXPECT_CALL(settings_manager_, UpdateDeviceSettings(_))
      .WillOnce([](mojom::DeviceSettingsPtr device_settings) {
        EXPECT_EQ(device_settings->static_headers.size(), 0u);
        EXPECT_EQ(device_settings->url_replacements.size(), 1u);
        EXPECT_EQ(device_settings->url_replacements[GURL("www.example.com")],
                  GURL("sub2.other-example.com"));
      });
  url_rewrite_adapter_->AddRenderFrame(std::move(remote));

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(UrlRewriteRulesAdapterTest, OneParam) {
  cast::v2::UrlRequestRewriteRules first_rules;
  {
    auto* rule = first_rules.add_rules();
    rule->set_action(
        cast::v2::UrlRequestRewriteRule_UrlRequestAction_ACTION_UNSPECIFIED);
    rule->add_host_filters("*.wild-example.com");
    rule->add_host_filters("www.exact-example.com");
    rule->add_scheme_filters("https");
    auto* add_headers = rule->add_rewrites()->mutable_add_headers();
    auto* headers = add_headers->add_headers();
    headers->set_name("SomeHeader");
    headers->set_value("value7");
    auto* remove_replace = rule->add_rewrites()->mutable_remove_header();
    auto* remove_suppress = rule->add_rewrites()->mutable_remove_header();
    auto* subst_replace =
        rule->add_rewrites()->mutable_substitute_query_pattern();
    auto* subst_suppress =
        rule->add_rewrites()->mutable_substitute_query_pattern();
    auto* subst_amp = rule->add_rewrites()->mutable_substitute_query_pattern();
    remove_replace->set_header_name("SomeHeader");
    remove_replace->set_query_pattern("${SomeHeader}");
    remove_suppress->set_header_name("SomeHeader");
    remove_suppress->set_query_pattern("${~SomeHeader}");
    subst_replace->set_pattern("${SomeHeader}");
    subst_replace->set_substitution("value7");
    subst_suppress->set_pattern("${~SomeHeader}");
    subst_suppress->set_substitution("");
    subst_amp->set_pattern("&&");
    subst_amp->set_substitution("&");
  }

  mojo::AssociatedRemote<mojom::IdentificationSettingsManager> remote(
      settings_manager_.Bind());
  url_rewrite_adapter_ = std::make_unique<UrlRewriteRulesAdapter>(first_rules);
  EXPECT_CALL(settings_manager_, UpdateAppSettings(_))
      .WillOnce([](mojom::AppSettingsPtr app_settings) {
        EXPECT_EQ(app_settings->full_host_names.size(), 2u);
        EXPECT_EQ(app_settings->wildcard_host_names.size(), 1u);
      });
  EXPECT_CALL(settings_manager_, SetSubstitutableParameters(_))
      .WillOnce([](std::vector<mojom::SubstitutableParameterPtr> params) {
        ASSERT_EQ(params.size(), 1u);
        EXPECT_EQ(params[0]->name, "SomeHeader");
        EXPECT_EQ(params[0]->replacement_token, "${SomeHeader}");
        EXPECT_EQ(params[0]->suppression_token, "${~SomeHeader}");
        EXPECT_EQ(params[0]->value, "value7");
      });
  url_rewrite_adapter_->AddRenderFrame(std::move(remote));

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

}  // namespace chromecast
