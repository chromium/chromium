// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_support_android.h"
#include "base/threading/thread.h"
#include "components/cronet/android/test/cronet_test_util.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/test/test_data_directory.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_backend_response.h"
#include "net/third_party/quiche/src/quiche/quic/tools/quic_memory_cache_backend.h"
#include "net/tools/quic/quic_simple_server.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cronet/android/cronet_test_apk_jni/QuicTestServer_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace cronet {

namespace {

static const int kServerPort = 6121;
static const std::string kConnectionClosePath = "/close_connection";

std::unique_ptr<base::Thread> g_quic_server_thread;
std::unique_ptr<quic::QuicMemoryCacheBackend> g_quic_memory_cache_backend;
std::unique_ptr<net::QuicSimpleServer> g_quic_server;
base::WaitableEvent wait_for_callback(
    base::WaitableEvent::ResetPolicy::AUTOMATIC);

template <typename T>
base::OnceCallback<void()> WrapCallbackWithSignal(
    base::OnceCallback<T> callback) {
  return base::BindOnce(base::IgnoreResult(std::move(callback)))
      .Then(base::BindOnce([] { wait_for_callback.Signal(); }));
}

template <typename T>
void ExecuteSynchronouslyOnServerThread(base::OnceCallback<T> callback,
                                        base::Location from_here = FROM_HERE) {
  CHECK(g_quic_server_thread);
  g_quic_server_thread->task_runner()->PostTask(
      from_here, WrapCallbackWithSignal(std::move(callback)));
  wait_for_callback.Wait();
}

void StartOnServerThread(const base::FilePath& test_files_root,
                         const base::FilePath& test_data_dir) {
  CHECK(g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  CHECK(!g_quic_server);
  CHECK(!g_quic_memory_cache_backend);

  // Set up in-memory cache.
  base::FilePath file_dir = test_files_root.Append("quic_data");
  CHECK(base::PathExists(file_dir)) << "Quic data does not exist";
  g_quic_memory_cache_backend =
      std::make_unique<quic::QuicMemoryCacheBackend>();
  g_quic_memory_cache_backend->InitializeBackend(file_dir.value());
  quic::QuicConfig config;

  // Set up server certs.
  base::FilePath directory = test_data_dir.Append("net/data/ssl/certificates");
  std::unique_ptr<net::ProofSourceChromium> proof_source(
      new net::ProofSourceChromium());
  CHECK(proof_source->Initialize(directory.Append("quic-chain.pem"),
                                 directory.Append("quic-leaf-cert.key"),
                                 base::FilePath()));
  g_quic_server = std::make_unique<net::QuicSimpleServer>(
      std::move(proof_source), config,
      quic::QuicCryptoServerConfig::ConfigOptions(),
      quic::AllSupportedVersions(), g_quic_memory_cache_backend.get());

  // Start listening.
  bool rv = g_quic_server->Listen(
      net::IPEndPoint(net::IPAddress::IPv4AllZeros(), kServerPort));
  if (rv) {
    // TODO(crbug.com/40283192): Stop hardcoding server hostname.
    g_quic_memory_cache_backend->AddSpecialResponse(
        base::StringPrintf("%s:%d", "test.example.com", kServerPort),
        kConnectionClosePath,
        quic::QuicBackendResponse::SpecialResponseType::CLOSE_CONNECTION);
  }
  CHECK_GE(rv, 0) << "Quic server fails to start";
}

void SetResponseDelayOnServerThread(const std::string& path,
                                    base::TimeDelta delay) {
  CHECK(g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  CHECK(g_quic_memory_cache_backend);

  // TODO(crbug.com/40283192): Stop hardcoding server hostname.
  CHECK(g_quic_memory_cache_backend->SetResponseDelay(
      base::StringPrintf("%s:%d", "test.example.com", kServerPort), path,
      quic::QuicTime::Delta::FromMicroseconds(delay.InMicroseconds())));
}

void ShutdownOnServerThread() {
  CHECK(g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  CHECK(g_quic_server);
  CHECK(g_quic_memory_cache_backend);
  g_quic_server->Shutdown();
  g_quic_server.reset();
  g_quic_memory_cache_backend.reset();
}

}  // namespace

// Quic server is currently hardcoded to run on port 6121 of the localhost on
// the device.
void JNI_QuicTestServer_StartQuicTestServer(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtest_files_root,
    const JavaParamRef<jstring>& jtest_data_dir) {
  CHECK(!g_quic_server_thread);
  base::FilePath test_data_dir(
      base::android::ConvertJavaStringToUTF8(env, jtest_data_dir));
  base::InitAndroidTestPaths(test_data_dir);
  g_quic_server_thread = std::make_unique<base::Thread>("quic server thread");
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  bool started =
      g_quic_server_thread->StartWithOptions(std::move(thread_options));
  CHECK(started);
  base::FilePath test_files_root(
      base::android::ConvertJavaStringToUTF8(env, jtest_files_root));
  ExecuteSynchronouslyOnServerThread(
      base::BindOnce(&StartOnServerThread, test_files_root, test_data_dir));
}

ScopedJavaLocalRef<jstring> JNI_QuicTestServer_GetConnectionClosePath(
    JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(env, kConnectionClosePath);
}

void JNI_QuicTestServer_ShutdownQuicTestServer(JNIEnv* env) {
  CHECK(!g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  ExecuteSynchronouslyOnServerThread(base::BindOnce(&ShutdownOnServerThread));
  g_quic_server_thread.reset();
}

int JNI_QuicTestServer_GetServerPort(JNIEnv* env) {
  return kServerPort;
}

void JNI_QuicTestServer_DelayResponse(JNIEnv* env,
                                      const JavaParamRef<jstring>& jpath,
                                      int delayInSeconds) {
  CHECK(!g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  std::string path = base::android::ConvertJavaStringToUTF8(env, jpath);
  base::TimeDelta delay = base::Seconds(delayInSeconds);
  ExecuteSynchronouslyOnServerThread(
      base::BindOnce(&SetResponseDelayOnServerThread, path, delay));
}

}  // namespace cronet
