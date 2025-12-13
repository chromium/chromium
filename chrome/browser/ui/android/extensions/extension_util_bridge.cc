// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/extensions/extension_util_bridge.h"

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/files/file_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/extensions/jni_headers/ExtensionUtilBridge_jni.h"

namespace extensions {

std::optional<base::FilePath> GetFileUnderDownloads(
    const std::string& file_name) {
  if (file_name.empty()) {
    return std::nullopt;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string uri =
      Java_ExtensionUtilBridge_getFileUnderDownloads(env, file_name);
  if (uri.empty()) {
    return std::nullopt;
  }
  return base::FilePath(uri);
}

std::optional<std::vector<base::FilePath>> GetOrCreateEmptyFilesUnderDownloads(
    const base::FilePath& file_for_basename,
    const std::vector<std::string>& dot_extensions) {
  auto content_uri = base::ResolveToContentUri(file_for_basename);
  if (!content_uri) {
    return std::nullopt;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::string> uris =
      Java_ExtensionUtilBridge_getOrCreateEmptyFilesUnderDownloads(
          env, content_uri->value(), dot_extensions);
  if (uris.empty()) {
    return std::nullopt;
  }

  std::vector<base::FilePath> result;
  for (auto uri : uris) {
    result.emplace_back(uri);
  }
  return result;
}

}  // namespace extensions

DEFINE_JNI(ExtensionUtilBridge)
