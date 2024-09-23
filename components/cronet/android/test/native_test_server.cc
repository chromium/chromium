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
#include "base/test/test_support_android.h"
#include "components/cronet/testing/test_server/test_server.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/cronet/android/cronet_test_apk_jni/NativeTestServer_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace cronet {

jboolean JNI_NativeTestServer_StartNativeTestServer(
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
  return cronet::TestServer::StartServeFilesFromDirectory(
      test_files_root,
      (juse_https ? net::test_server::EmbeddedTestServer::TYPE_HTTPS
                  : net::test_server::EmbeddedTestServer::TYPE_HTTP),
      static_cast<net::EmbeddedTestServer::ServerCertificate>(
          jserver_certificate));
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

}  // namespace cronet
