// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/test/cronet_test_util.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cronet/android/cronet_tests_jni_headers/CronetTestUtil_jni.h"
#include "components/cronet/android/cronet_url_request_adapter.h"
#include "components/cronet/android/cronet_url_request_context_adapter.h"
#include "components/cronet/cronet_url_request.h"
#include "components/cronet/cronet_url_request_context.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request.h"

namespace cronet {
namespace {

using ::base::MessagePump;
using ::base::MessagePumpType;
using ::base::android::JavaParamRef;
using ::base::sequence_manager::SequenceManager;

SequenceManager* g_sequence_manager = nullptr;

}  // namespace

jint JNI_CronetTestUtil_GetLoadFlags(JNIEnv* env,
                                     const jlong jurl_request_adapter) {
  return TestUtil::GetURLRequest(jurl_request_adapter)->load_flags();
}

// static
scoped_refptr<base::SingleThreadTaskRunner> TestUtil::GetTaskRunner(
    jlong jcontext_adapter) {
  CronetURLRequestContextAdapter* context_adapter =
      reinterpret_cast<CronetURLRequestContextAdapter*>(jcontext_adapter);
  return context_adapter->context_->network_task_runner_;
}

// static
net::URLRequestContext* TestUtil::GetURLRequestContext(jlong jcontext_adapter) {
  CronetURLRequestContextAdapter* context_adapter =
      reinterpret_cast<CronetURLRequestContextAdapter*>(jcontext_adapter);
  return context_adapter->context_->network_tasks_->context_.get();
}

// static
void TestUtil::RunAfterContextInitOnNetworkThread(jlong jcontext_adapter,
                                                  const base::Closure& task) {
  CronetURLRequestContextAdapter* context_adapter =
      reinterpret_cast<CronetURLRequestContextAdapter*>(jcontext_adapter);
  if (context_adapter->context_->network_tasks_->is_context_initialized_) {
    task.Run();
  } else {
    context_adapter->context_->network_tasks_->tasks_waiting_for_context_.push(
        task);
  }
}

// static
void TestUtil::RunAfterContextInit(jlong jcontext_adapter,
                                   const base::Closure& task) {
  GetTaskRunner(jcontext_adapter)
      ->PostTask(FROM_HERE,
                 base::BindOnce(&TestUtil::RunAfterContextInitOnNetworkThread,
                                jcontext_adapter, task));
}

// static
net::URLRequest* TestUtil::GetURLRequest(jlong jrequest_adapter) {
  CronetURLRequestAdapter* request_adapter =
      reinterpret_cast<CronetURLRequestAdapter*>(jrequest_adapter);
  return request_adapter->request_->network_tasks_.url_request_.get();
}

static void PrepareNetworkThreadOnNetworkThread(jlong jcontext_adapter) {
  g_sequence_manager =
      base::sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
          MessagePump::Create(MessagePumpType::IO),
          SequenceManager::Settings::Builder()
              .SetMessagePumpType(MessagePumpType::IO)
              .Build())
          .release();
  g_sequence_manager->SetDefaultTaskRunner(
      TestUtil::GetTaskRunner(jcontext_adapter));
}

// Tests need to call into libcronet.so code on libcronet.so threads.
// libcronet.so's threads are registered with static tables for MessageLoops
// and SingleThreadTaskRunners in libcronet.so, so libcronet_test.so
// functions that try and access these tables will find missing entries in
// the corresponding static tables in libcronet_test.so.  Fix this by
// initializing a MessageLoop and SingleThreadTaskRunner in libcronet_test.so
// for these threads.  Called from Java CronetTestUtil class.
void JNI_CronetTestUtil_PrepareNetworkThread(
    JNIEnv* env,
    jlong jcontext_adapter) {
  TestUtil::GetTaskRunner(jcontext_adapter)
      ->PostTask(FROM_HERE, base::BindOnce(&PrepareNetworkThreadOnNetworkThread,
                                           jcontext_adapter));
}

static void CleanupNetworkThreadOnNetworkThread() {
  DCHECK(g_sequence_manager);
  delete g_sequence_manager;
  g_sequence_manager = nullptr;
}

// Called from Java CronetTestUtil class.
void JNI_CronetTestUtil_CleanupNetworkThread(
    JNIEnv* env,
    jlong jcontext_adapter) {
  TestUtil::RunAfterContextInit(
      jcontext_adapter, base::Bind(&CleanupNetworkThreadOnNetworkThread));
}

jboolean JNI_CronetTestUtil_CanGetTaggedBytes(JNIEnv* env) {
  return net::CanGetTaggedBytes();
}

jlong JNI_CronetTestUtil_GetTaggedBytes(JNIEnv* env,
                                        jint jexpected_tag) {
  return net::GetTaggedBytes(jexpected_tag);
}

}  // namespace cronet
