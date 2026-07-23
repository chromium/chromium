// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/consent_kit/consent_kit_url_builder.h"

#include <optional>
#include <string>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/uuid.h"
#include "base/values.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace drive {

TEST(ConsentKitUrlBuilderTest, BuildMinimalUrl) {
  ConsentKitUrlBuilder builder;
  builder.SetSessionIndex(1);
  builder.SetLocale("fr");

  GURL url = builder.Build();

  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ(url.scheme(), "https");
  EXPECT_EQ(url.host(), "consent.google.com");
  EXPECT_EQ(url.path(), "/signedin/landing");

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "hl", &value));
  EXPECT_EQ(value, "fr");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "authuser", &value));
  EXPECT_EQ(value, "1");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "origin", &value));
  EXPECT_EQ(value, "chrome-untrusted://drive-picker-host");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "ppc", &value));
  EXPECT_FALSE(value.empty());

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "theme", &value));
  EXPECT_EQ(value, "1");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "color_scheme", &value));
  EXPECT_EQ(value, "light");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "ppc", &value));

  std::optional<base::Value> parsed_ppc =
      base::JSONReader::Read(value, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_ppc.has_value());
  ASSERT_TRUE(parsed_ppc->is_list());
  const base::ListValue& ppc_list = parsed_ppc->GetList();
  ASSERT_EQ(ppc_list.size(), 5u);

  // Field 0: FlowParams [flow_id]
  ASSERT_TRUE(ppc_list[0].is_list());
  const base::ListValue& flow_params = ppc_list[0].GetList();
  ASSERT_EQ(flow_params.size(), 1u);
  EXPECT_EQ(flow_params[0].GetInt(), 0);  // default flow_id

  // Field 1: ProductEntryPoint [product_id, product_surface,
  // entrypoint_opaque_id]
  ASSERT_TRUE(ppc_list[1].is_list());
  const base::ListValue& product_entry_point = ppc_list[1].GetList();
  ASSERT_EQ(product_entry_point.size(), 3u);
  EXPECT_EQ(product_entry_point[0].GetInt(), 0);  // default product_id
  EXPECT_EQ(product_entry_point[1].GetInt(),
            1);  // product_surface (DEMO_UI_SURFACE)
  EXPECT_EQ(product_entry_point[2].GetString(), "");  // default entrypoint_id

  // Field 2: SessionInfo [[null, null, uuid_string]]
  ASSERT_TRUE(ppc_list[2].is_list());
  const base::ListValue& session_info = ppc_list[2].GetList();
  ASSERT_EQ(session_info.size(), 1u);
  ASSERT_TRUE(session_info[0].is_list());
  const base::ListValue& shared_consent_session_id = session_info[0].GetList();
  ASSERT_EQ(shared_consent_session_id.size(), 3u);
  EXPECT_TRUE(shared_consent_session_id[0].is_none());
  EXPECT_TRUE(shared_consent_session_id[1].is_none());
  std::string uuid_string = shared_consent_session_id[2].GetString();
  EXPECT_TRUE(base::Uuid::ParseLowercase(uuid_string).is_valid());

  // Field 3: PresentationParams [locale, color_theme]
  ASSERT_TRUE(ppc_list[3].is_list());
  const base::ListValue& presentation_params = ppc_list[3].GetList();
  ASSERT_EQ(presentation_params.size(), 2u);
  EXPECT_EQ(presentation_params[0].GetString(), "fr");
  EXPECT_EQ(presentation_params[1].GetInt(), 1);  // default is LIGHT

  // Field 4: WebPlatformParams [[session_index], integration_type]
  ASSERT_TRUE(ppc_list[4].is_list());
  const base::ListValue& web_platform_params = ppc_list[4].GetList();
  ASSERT_EQ(web_platform_params.size(), 2u);
  ASSERT_TRUE(web_platform_params[0].is_list());
  const base::ListValue& user_info = web_platform_params[0].GetList();
  ASSERT_EQ(user_info.size(), 1u);
  EXPECT_EQ(user_info[0].GetInt(), 1);  // set via SetSessionIndex(1)
  EXPECT_EQ(web_platform_params[1].GetInt(),
            1);  // integration_type = WEB_SEARCH_EMBEDDED
}

TEST(ConsentKitUrlBuilderTest, BuildFullUrl) {
  ConsentKitUrlBuilder builder;
  builder.SetSessionIndex(2);
  builder.SetLocale("ja");
  builder.SetFlowId(987);
  builder.SetProductId(654);
  builder.SetEntrypointId("picker_entrypoint");
  builder.SetHostOrigins(
      {"chrome-untrusted://another-host", "chrome://yet-another-host"});

  GURL url = builder.Build();

  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ(url.scheme(), "https");
  EXPECT_EQ(url.host(), "consent.google.com");
  EXPECT_EQ(url.path(), "/signedin/landing");

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "hl", &value));
  EXPECT_EQ(value, "ja");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "authuser", &value));
  EXPECT_EQ(value, "2");

  EXPECT_NE(url.query().find("origin=chrome-untrusted%3A%2F%2Fanother-host"),
            std::string::npos);
  EXPECT_NE(url.query().find("origin=chrome%3A%2F%2Fyet-another-host"),
            std::string::npos);

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "ppc", &value));
  EXPECT_FALSE(value.empty());

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "theme", &value));
  EXPECT_EQ(value, "1");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "color_scheme", &value));
  EXPECT_EQ(value, "light");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "ppc", &value));

  std::optional<base::Value> parsed_ppc =
      base::JSONReader::Read(value, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_ppc.has_value());
  ASSERT_TRUE(parsed_ppc->is_list());
  const base::ListValue& ppc_list = parsed_ppc->GetList();
  ASSERT_EQ(ppc_list.size(), 5u);

  // Field 0: FlowParams [flow_id]
  ASSERT_TRUE(ppc_list[0].is_list());
  const base::ListValue& flow_params = ppc_list[0].GetList();
  ASSERT_EQ(flow_params.size(), 1u);
  EXPECT_EQ(flow_params[0].GetInt(), 987);

  // Field 1: ProductEntryPoint [product_id, product_surface,
  // entrypoint_opaque_id]
  ASSERT_TRUE(ppc_list[1].is_list());
  const base::ListValue& product_entry_point = ppc_list[1].GetList();
  ASSERT_EQ(product_entry_point.size(), 3u);
  EXPECT_EQ(product_entry_point[0].GetInt(), 654);
  EXPECT_EQ(product_entry_point[1].GetInt(), 1);
  EXPECT_EQ(product_entry_point[2].GetString(), "picker_entrypoint");

  // Field 2: SessionInfo [[null, null, uuid_string]]
  ASSERT_TRUE(ppc_list[2].is_list());
  const base::ListValue& session_info = ppc_list[2].GetList();
  ASSERT_EQ(session_info.size(), 1u);
  ASSERT_TRUE(session_info[0].is_list());
  const base::ListValue& shared_consent_session_id = session_info[0].GetList();
  ASSERT_EQ(shared_consent_session_id.size(), 3u);
  EXPECT_TRUE(shared_consent_session_id[0].is_none());
  EXPECT_TRUE(shared_consent_session_id[1].is_none());
  std::string uuid_string = shared_consent_session_id[2].GetString();
  EXPECT_TRUE(base::Uuid::ParseLowercase(uuid_string).is_valid());

  // Field 3: PresentationParams [locale, color_theme]
  ASSERT_TRUE(ppc_list[3].is_list());
  const base::ListValue& presentation_params = ppc_list[3].GetList();
  ASSERT_EQ(presentation_params.size(), 2u);
  EXPECT_EQ(presentation_params[0].GetString(), "ja");
  EXPECT_EQ(presentation_params[1].GetInt(), 1);  // default is LIGHT

  // Field 4: WebPlatformParams [[session_index], integration_type]
  ASSERT_TRUE(ppc_list[4].is_list());
  const base::ListValue& web_platform_params = ppc_list[4].GetList();
  ASSERT_EQ(web_platform_params.size(), 2u);
  ASSERT_TRUE(web_platform_params[0].is_list());
  const base::ListValue& user_info = web_platform_params[0].GetList();
  ASSERT_EQ(user_info.size(), 1u);
  EXPECT_EQ(user_info[0].GetInt(),
            2);  // session_index set via SetSessionIndex(2)
  EXPECT_EQ(web_platform_params[1].GetInt(),
            1);  // integration_type = WEB_SEARCH_EMBEDDED
}

TEST(ConsentKitUrlBuilderTest, BuildUrlWithDarkMode) {
  ConsentKitUrlBuilder builder;
  builder.SetDarkMode(true);

  GURL url = builder.Build();

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "theme", &value));
  EXPECT_EQ(value, "2");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "color_scheme", &value));
  EXPECT_EQ(value, "dark");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "ppc", &value));
  std::optional<base::Value> parsed_ppc =
      base::JSONReader::Read(value, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_ppc.has_value());
  const base::ListValue& ppc_list = parsed_ppc->GetList();
  ASSERT_EQ(ppc_list.size(), 5u);

  // Field 3: PresentationParams [locale, color_theme]
  const base::ListValue& presentation_params = ppc_list[3].GetList();
  ASSERT_EQ(presentation_params.size(), 2u);
  EXPECT_EQ(presentation_params[1].GetInt(), 2);  // 2 is DARK
}

}  // namespace drive
