// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/tab_context_decryption_token_extension.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/sync/base/features.h"
#include "components/sync_tab_context/http_rpc_constants.h"
#include "gin/array_buffer.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"

namespace {

using TabContextDecryptionTokenExtensionTest = gin::V8Test;

TEST_F(TabContextDecryptionTokenExtensionTest,
       ShouldExposeTabContextJavascriptApi) {
  const url::Origin kAllowedOrigin =
      url::Origin::Create(GURL("https://chromestorage.goog"));
  const url::Origin kDisallowedOrigin =
      url::Origin::Create(GURL("https://example.com"));

  base::test::ScopedFeatureList scoped_feature_list;

  // Feature disabled -> false.
  scoped_feature_list.InitAndDisableFeature(
      syncer::kSyncEncryptedTabContextContainer);
  EXPECT_FALSE(TabContextDecryptionTokenExtension::
                   ShouldExposeTabContextJavascriptApiForTesting(
                       kAllowedOrigin, /*is_locked_to_site=*/true));

  // Feature enabled...
  scoped_feature_list.Reset();
  scoped_feature_list.InitAndEnableFeature(
      syncer::kSyncEncryptedTabContextContainer);

  // Feature enabled, wrong origin -> false.
  EXPECT_FALSE(TabContextDecryptionTokenExtension::
                   ShouldExposeTabContextJavascriptApiForTesting(
                       kDisallowedOrigin, /*is_locked_to_site=*/true));

  // Feature enabled, allowed origin, not locked to site -> false.
  EXPECT_FALSE(TabContextDecryptionTokenExtension::
                   ShouldExposeTabContextJavascriptApiForTesting(
                       kAllowedOrigin, /*is_locked_to_site=*/false));

  // Feature enabled, allowed origin, locked to site -> true.
  EXPECT_TRUE(TabContextDecryptionTokenExtension::
                  ShouldExposeTabContextJavascriptApiForTesting(
                      kAllowedOrigin, /*is_locked_to_site=*/true));

  // Overridden origin via command line switch.
  const url::Origin kCustomOrigin =
      url::Origin::Create(GURL("https://custom.chromestorage.test"));
  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        sync_tab_context::kTabContextAllowedOriginSwitch,
        kCustomOrigin.GetURL().spec());
    EXPECT_TRUE(TabContextDecryptionTokenExtension::
                    ShouldExposeTabContextJavascriptApiForTesting(
                        kCustomOrigin, /*is_locked_to_site=*/true));
    EXPECT_FALSE(TabContextDecryptionTokenExtension::
                     ShouldExposeTabContextJavascriptApiForTesting(
                         kAllowedOrigin, /*is_locked_to_site=*/true));
  }
}

TEST_F(TabContextDecryptionTokenExtensionTest, CreateTokenValue) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate, context_);
  v8::Context::Scope context_scope(context);

  // std::nullopt -> null.
  v8::Local<v8::Value> null_value =
      TabContextDecryptionTokenExtension::CreateTokenValueForTesting(
          isolate, std::nullopt);
  EXPECT_TRUE(null_value->IsNull());

  // Empty vector -> 0 byte buffer.
  std::vector<uint8_t> empty_bytes;
  v8::Local<v8::Value> empty_buffer_val =
      TabContextDecryptionTokenExtension::CreateTokenValueForTesting(
          isolate, empty_bytes);
  ASSERT_TRUE(empty_buffer_val->IsArrayBuffer());
  EXPECT_EQ(0u, empty_buffer_val.As<v8::ArrayBuffer>()->ByteLength());

  // Non-empty vector -> array buffer with matching contents.
  const std::string kSecret = "secret_token_123";
  std::vector<uint8_t> token_bytes(kSecret.begin(), kSecret.end());
  v8::Local<v8::Value> buffer_val =
      TabContextDecryptionTokenExtension::CreateTokenValueForTesting(
          isolate, token_bytes);
  ASSERT_TRUE(buffer_val->IsArrayBuffer());
  v8::Local<v8::ArrayBuffer> buffer = buffer_val.As<v8::ArrayBuffer>();
  EXPECT_EQ(kSecret.size(), buffer->ByteLength());
  EXPECT_EQ(kSecret, base::as_string_view(gin::ArrayBuffer(buffer).span()));
}

}  // namespace
