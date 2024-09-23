// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/cronet/android/test/cronet_test_util.h"
#include "components/cronet/url_request_context_config.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/url_request/url_request_context.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cronet/android/cronet_tests_jni_headers/ExperimentalOptionsTest_jni.h"

using base::android::JavaParamRef;

namespace cronet {

namespace {
void WriteToHostCacheOnNetworkThread(jlong jcontext_adapter,
                                     const std::string& address_string) {
  net::URLRequestContext* context =
      TestUtil::GetURLRequestContext(jcontext_adapter);
  net::HostCache* cache = context->host_resolver()->GetHostCache();
  const std::string hostname = "host-cache-test-host";

  // Create multiple keys to ensure the test works in a variety of network
  // conditions.
  net::HostCache::Key key1(hostname, net::DnsQueryType::UNSPECIFIED, 0,
                           net::HostResolverSource::ANY,
                           net::NetworkAnonymizationKey());
  net::HostCache::Key key2(hostname, net::DnsQueryType::A,
                           net::HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6,
                           net::HostResolverSource::ANY,
                           net::NetworkAnonymizationKey());

  net::IPAddress address;
  CHECK(address.AssignFromIPLiteral(address_string));
  net::HostCache::Entry entry(net::OK, {{address, 0}}, /*aliases=*/{hostname},
                              net::HostCache::Entry::SOURCE_UNKNOWN);
  cache->Set(key1, entry, base::TimeTicks::Now(), base::Seconds(1));
  cache->Set(key2, entry, base::TimeTicks::Now(), base::Seconds(1));
}
}  // namespace

static void JNI_ExperimentalOptionsTest_WriteToHostCache(
    JNIEnv* env,
    jlong jcontext_adapter,
    const JavaParamRef<jstring>& jaddress) {
  TestUtil::RunAfterContextInit(
      jcontext_adapter,
      base::BindOnce(&WriteToHostCacheOnNetworkThread, jcontext_adapter,
                     base::android::ConvertJavaStringToUTF8(env, jaddress)));
}

static jboolean
JNI_ExperimentalOptionsTest_ExperimentalOptionsParsingIsAllowedToFail(
    JNIEnv* env) {
  return URLRequestContextConfig::ExperimentalOptionsParsingIsAllowedToFail();
}

}  // namespace cronet
