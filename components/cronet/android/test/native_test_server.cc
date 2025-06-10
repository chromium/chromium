// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/test/test_support_android.h"
#include "components/cronet/testing/test_server/test_server.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace cronet {

using NativeTestServerHeaderMap =
    std::map<std::string,
             std::string,
             net::test_server::HttpRequest::CaseInsensitiveStringComparator>;

struct NativeTestServerHttpRequest final {
  net::test_server::HttpRequest http_request;
};

struct NativeTestServerRawHttpResponse final {
  std::unique_ptr<net::test_server::RawHttpResponse> raw_http_response;
};

class NativeTestServerHandleRequestCallback final {
 public:
  explicit NativeTestServerHandleRequestCallback(
      const jni_zero::JavaRef<jobject>& java_callback)
      : java_callback_(java_callback) {}

  std::unique_ptr<net::test_server::HttpResponse> operator()(
      const net::test_server::HttpRequest& http_request) const;

 private:
  jni_zero::ScopedJavaGlobalRef<jobject> java_callback_;
};

}  // namespace cronet

namespace jni_zero {

template <>
jni_zero::ScopedJavaLocalRef<jobject>
ToJniType<cronet::NativeTestServerHttpRequest>(
    JNIEnv* env,
    const cronet::NativeTestServerHttpRequest& input);
template <>
cronet::NativeTestServerRawHttpResponse
FromJniType<cronet::NativeTestServerRawHttpResponse>(
    JNIEnv* env,
    const JavaRef<jobject>& java_raw_http_response);
template <>
std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>
FromJniType<std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>>(
    JNIEnv* env,
    const JavaRef<jobject>& java_handle_request_callback);

}  // namespace jni_zero

// Uses the declarations above, so must come after them.
#include "components/cronet/android/cronet_test_apk_jni/NativeTestServer_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace jni_zero {

template <>
jni_zero::ScopedJavaLocalRef<jobject>
ToJniType<cronet::NativeTestServerHttpRequest>(
    JNIEnv* env,
    const cronet::NativeTestServerHttpRequest&
        native_test_server_http_request) {
  const auto& http_request = native_test_server_http_request.http_request;
  return cronet::Java_NativeTestServer_createHttpRequest(
      env, http_request.relative_url, http_request.headers,
      http_request.method_string, http_request.all_headers,
      http_request.content);
}

template <>
cronet::NativeTestServerRawHttpResponse
FromJniType<cronet::NativeTestServerRawHttpResponse>(
    JNIEnv* env,
    const JavaRef<jobject>& java_raw_http_response) {
  return {.raw_http_response =
              std::make_unique<net::test_server::RawHttpResponse>(
                  cronet::Java_NativeTestServer_getRawHttpResponseHeaders(
                      env, java_raw_http_response),
                  cronet::Java_NativeTestServer_getRawHttpResponseContents(
                      env, java_raw_http_response))};
}

template <>
std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>
FromJniType<std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>>(
    JNIEnv* env,
    const JavaRef<jobject>& java_handle_request_callback) {
  return std::make_unique<cronet::NativeTestServerHandleRequestCallback>(
      java_handle_request_callback);
}

}  // namespace jni_zero

namespace cronet {

std::unique_ptr<net::test_server::HttpResponse>
NativeTestServerHandleRequestCallback::operator()(
    const net::test_server::HttpRequest& http_request) const {
  cronet::Java_NativeTestServer_handleRequest(jni_zero::AttachCurrentThread(),
                                              java_callback_,
                                              {.http_request = http_request});
  return nullptr;
}

jboolean JNI_NativeTestServer_PrepareNativeTestServer(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtest_files_root,
    const JavaParamRef<jstring>& jtest_data_dir,
    jboolean juse_https,
    jint jserver_certificate) {
  base::FilePath test_data_dir(
      base::android::ConvertJavaStringToUTF8(env, jtest_data_dir));
  base::InitAndroidTestPaths(test_data_dir);

  base::FilePath test_files_root(
      base::android::ConvertJavaStringToUTF8(env, jtest_files_root));
  return cronet::TestServer::PrepareServeFilesFromDirectory(
      test_files_root,
      (juse_https ? net::test_server::EmbeddedTestServer::TYPE_HTTPS
                  : net::test_server::EmbeddedTestServer::TYPE_HTTP),
      static_cast<net::EmbeddedTestServer::ServerCertificate>(
          jserver_certificate));
}

void JNI_NativeTestServer_StartPrepared(JNIEnv* env) {
  cronet::TestServer::StartPrepared();
}

void JNI_NativeTestServer_ShutdownNativeTestServer(JNIEnv* env) {
  cronet::TestServer::Shutdown();
}

ScopedJavaLocalRef<jstring> JNI_NativeTestServer_GetEchoBodyURL(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, cronet::TestServer::GetEchoRequestBodyURL());
}

ScopedJavaLocalRef<jstring> JNI_NativeTestServer_GetEchoHeaderURL(
    JNIEnv* env,
    const JavaParamRef<jstring>& jheader) {
  return base::android::ConvertUTF8ToJavaString(
      env, cronet::TestServer::GetEchoHeaderURL(
               base::android::ConvertJavaStringToUTF8(env, jheader)));
}

ScopedJavaLocalRef<jstring> JNI_NativeTestServer_GetEchoAllHeadersURL(
    JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, cronet::TestServer::GetEchoAllHeadersURL());
}

ScopedJavaLocalRef<jstring> JNI_NativeTestServer_GetEchoMethodURL(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, cronet::TestServer::GetEchoMethodURL());
}

ScopedJavaLocalRef<jstring> JNI_NativeTestServer_GetRedirectToEchoBody(
    JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, cronet::TestServer::GetRedirectToEchoBodyURL());
}

ScopedJavaLocalRef<jstring> JNI_NativeTestServer_GetFileURL(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfile_path) {
  return base::android::ConvertUTF8ToJavaString(
      env, cronet::TestServer::GetFileURL(
               base::android::ConvertJavaStringToUTF8(env, jfile_path)));
}

jint JNI_NativeTestServer_GetPort(JNIEnv* env) {
  return cronet::TestServer::GetPort();
}

ScopedJavaLocalRef<jstring> JNI_NativeTestServer_GetExabyteResponseURL(
    JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, cronet::TestServer::GetExabyteResponseURL());
}

ScopedJavaLocalRef<jstring> JNI_NativeTestServer_GetHostPort(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, cronet::TestServer::GetHostPort());
}

void JNI_NativeTestServer_RegisterRequestHandler(
    JNIEnv* env,
    std::unique_ptr<cronet::NativeTestServerHandleRequestCallback>& callback) {
  cronet::TestServer::RegisterRequestHandler(base::BindRepeating(
      &cronet::NativeTestServerHandleRequestCallback::operator(),
      base::Owned(std::move(callback))));
}

}  // namespace cronet
