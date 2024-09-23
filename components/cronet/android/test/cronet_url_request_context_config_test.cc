// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cronet_url_request_context_config_test.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "components/cronet/url_request_context_config.h"
#include "components/cronet/version.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cronet/android/cronet_tests_jni_headers/CronetUrlRequestContextTest_jni.h"

using base::android::JavaParamRef;

namespace cronet {

// Verifies that all the configuration options set by
// CronetUrlRequestContextTest.testCronetEngineBuilderConfig
// made it from the CronetEngine.Builder to the URLRequestContextConfig.
static void JNI_CronetUrlRequestContextTest_VerifyUrlRequestContextConfig(
    JNIEnv* env,
    jlong jurl_request_context_config,
    const JavaParamRef<jstring>& jstorage_path) {
  URLRequestContextConfig* config =
      reinterpret_cast<URLRequestContextConfig*>(jurl_request_context_config);
  CHECK_EQ(config->enable_spdy, false);
  CHECK_EQ(config->enable_quic, true);
  CHECK_EQ(config->bypass_public_key_pinning_for_local_trust_anchors, false);
  CHECK_EQ(config->quic_hints.size(), 1u);
  CHECK_EQ((*config->quic_hints.begin())->host, "example.com");
  CHECK_EQ((*config->quic_hints.begin())->port, 12);
  CHECK_EQ((*config->quic_hints.begin())->alternate_port, 34);
  CHECK_EQ(config->load_disable_cache, false);
  CHECK_EQ(config->http_cache, URLRequestContextConfig::HttpCacheType::MEMORY);
  CHECK_EQ(config->http_cache_max_size, 54321);
  CHECK_EQ(config->user_agent, "efgh");
  CHECK(config->effective_experimental_options.empty());
  CHECK_EQ(config->storage_path,
           base::android::ConvertJavaStringToUTF8(env, jstorage_path));
}

// Verify that QUIC can be turned off in CronetEngine.Builder.
// Note that the config expectation is hard coded here because it's very hard to
// create an expected config in the JAVA package.
static void
JNI_CronetUrlRequestContextTest_VerifyUrlRequestContextQuicOffConfig(
    JNIEnv* env,
    jlong jurl_request_context_config,
    const JavaParamRef<jstring>& jstorage_path) {
  URLRequestContextConfig* config =
      reinterpret_cast<URLRequestContextConfig*>(jurl_request_context_config);
  CHECK_EQ(config->enable_spdy, false);
  CHECK_EQ(config->enable_quic, false);
  CHECK_EQ(config->bypass_public_key_pinning_for_local_trust_anchors, false);
  CHECK_EQ(config->load_disable_cache, false);
  CHECK_EQ(config->http_cache, URLRequestContextConfig::HttpCacheType::MEMORY);
  CHECK_EQ(config->http_cache_max_size, 54321);
  CHECK_EQ(config->user_agent, "efgh");
  CHECK(config->effective_experimental_options.empty());
  CHECK_EQ(config->storage_path,
           base::android::ConvertJavaStringToUTF8(env, jstorage_path));
}

}  // namespace cronet
