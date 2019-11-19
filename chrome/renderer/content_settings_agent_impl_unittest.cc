// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/content_settings_agent_impl.h"

#include "chrome/common/url_constants.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

using blink::WebSecurityOrigin;

typedef testing::Test ContentSettingsAgentImplTest;

TEST_F(ContentSettingsAgentImplTest, WhitelistedSchemes) {
  std::string end_url = ":something";

  GURL chrome_ui_url =
      GURL(std::string(content::kChromeUIScheme).append(end_url));
  EXPECT_TRUE(ContentSettingsAgentImpl::IsWhitelistedForContentSettings(
      WebSecurityOrigin::Create(chrome_ui_url), GURL()));

  GURL chrome_dev_tools_url =
      GURL(std::string(content::kChromeDevToolsScheme).append(end_url));
  EXPECT_TRUE(ContentSettingsAgentImpl::IsWhitelistedForContentSettings(
      WebSecurityOrigin::Create(chrome_dev_tools_url), GURL()));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  GURL extension_url =
      GURL(std::string(extensions::kExtensionScheme).append(end_url));
  EXPECT_TRUE(ContentSettingsAgentImpl::IsWhitelistedForContentSettings(
      WebSecurityOrigin::Create(extension_url), GURL()));
#endif

  GURL file_url("file:///dir/");
  EXPECT_TRUE(ContentSettingsAgentImpl::IsWhitelistedForContentSettings(
      WebSecurityOrigin::Create(file_url), GURL("file:///dir/")));
  EXPECT_FALSE(ContentSettingsAgentImpl::IsWhitelistedForContentSettings(
      WebSecurityOrigin::Create(file_url), GURL("file:///dir/file")));

  GURL http_url = GURL("http://server.com/path");
  EXPECT_FALSE(ContentSettingsAgentImpl::IsWhitelistedForContentSettings(
      WebSecurityOrigin::Create(http_url), GURL()));
}
