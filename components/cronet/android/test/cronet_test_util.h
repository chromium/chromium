// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_TEST_CRONET_TEST_UTIL_H_
#define COMPONENTS_CRONET_ANDROID_TEST_CRONET_TEST_UTIL_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/network_handle.h"
#include "net/quic/quic_context.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_tag.h"

namespace net {
class URLRequest;
class URLRequestContext;
}  // namespace net

namespace cronet {

// Various test utility functions for testing Cronet.
// NOTE(pauljensen): This class is friended by Cronet internal implementation
// classes to provide access to internals.
class TestUtil {
 public:
  TestUtil() = delete;
  TestUtil(const TestUtil&) = delete;
  TestUtil& operator=(const TestUtil&) = delete;

  // CronetURLRequestContextAdapter manipulation:

  // Returns SingleThreadTaskRunner for the network thread of the context
  // adapter.
  static scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      int64_t jcontext_adapter);
  // Returns underlying default URLRequestContext.
  static net::URLRequestContext* GetURLRequestContext(int64_t jcontext_adapter);
  // Run |task| after URLRequestContext is initialized.
  static void RunAfterContextInit(int64_t jcontext_adapter,
                                  base::OnceClosure task);

  // CronetURLRequestAdapter manipulation:

  // Returns underlying URLRequest.
  static net::URLRequest* GetURLRequest(int64_t jrequest_adapter);

  // Returns underlying network to URLRequestContext map.
  static base::flat_map<net::handles::NetworkHandle,
                        std::unique_ptr<net::URLRequestContext>>*
  GetURLRequestContexts(int64_t jcontext_adapter);

  static net::QuicParams& GetDefaultURLRequestQuicParams(
      int64_t jcontext_adapter);

 private:
  static void RunAfterContextInitOnNetworkThread(int64_t jcontext_adapter,
                                                 base::OnceClosure task);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_TEST_CRONET_TEST_UTIL_H_
