// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_restrictions/browser/web_restrictions_client.h"

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "jni/WebRestrictionsClient_jni.h"

using base::android::ScopedJavaGlobalRef;

namespace web_restrictions {

namespace {

const size_t kMaxCacheSize = 100;

bool RequestPermissionTask(
    const std::string& url,
    const base::android::JavaRef<jobject>& java_provider) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_WebRestrictionsClient_requestPermission(
      env, java_provider,
      base::android::ConvertUTF8ToJavaString(env, url));
}

bool CheckSupportsRequestTask(
    const base::android::JavaRef<jobject>& java_provider) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_WebRestrictionsClient_supportsRequest(env, java_provider);
}

}  // namespace

WebRestrictionsClient::WebRestrictionsClient()
    : initialized_(false), supports_request_(false) {
  background_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

WebRestrictionsClient::~WebRestrictionsClient() {
  if (java_provider_.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebRestrictionsClient_onDestroy(env, java_provider_);
  java_provider_.Reset();
}

void WebRestrictionsClient::SetAuthority(
    const std::string& content_provider_authority) {
  // This is called from the UI thread, but class members should only be
  // accessed from the IO thread.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::Bind(&WebRestrictionsClient::SetAuthorityTask,
                 base::Unretained(this), content_provider_authority));
}

void WebRestrictionsClient::SetAuthorityTask(
    const std::string& content_provider_authority) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // Destroy any existing content resolver.
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_provider_.is_null()) {
    Java_WebRestrictionsClient_onDestroy(env, java_provider_);
    java_provider_.Reset();
  }
  ClearCache();
  provider_authority_ = content_provider_authority;

  // Initialize the content resolver.
  initialized_ = !content_provider_authority.empty();
  if (!initialized_)
    return;
  java_provider_.Reset(Java_WebRestrictionsClient_create(
      env,
      base::android::ConvertUTF8ToJavaString(env, content_provider_authority),
      reinterpret_cast<jlong>(this)));
  supports_request_ = false;
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&CheckSupportsRequestTask, java_provider_),
      base::Bind(&WebRestrictionsClient::RequestSupportKnown,
                 base::Unretained(this), provider_authority_));
}

UrlAccess WebRestrictionsClient::ShouldProceed(
    bool is_main_frame,
    const std::string& url,
    const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!initialized_)
    return ALLOW;

  std::unique_ptr<const WebRestrictionsClientResult> result =
      cache_.GetCacheEntry(url);
  if (result) {
    RecordURLAccess(url);
    return result->ShouldProceed() ? ALLOW : DISALLOW;
  }
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::Bind(&WebRestrictionsClient::ShouldProceedTask, url,
                 java_provider_),
      base::Bind(&WebRestrictionsClient::OnShouldProceedComplete,
                 base::Unretained(this), provider_authority_, url, callback));

  return PENDING;
}

bool WebRestrictionsClient::SupportsRequest() const {
  return initialized_ && supports_request_;
}

void WebRestrictionsClient::RequestPermission(
    const std::string& url,
    const base::Callback<void(bool)>& request_success) {
  if (!initialized_) {
    request_success.Run(false);
    return;
  }
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::Bind(&RequestPermissionTask, url, java_provider_), request_success);
}

void WebRestrictionsClient::OnWebRestrictionsChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::Bind(&WebRestrictionsClient::ClearCache, base::Unretained(this)));
}

void WebRestrictionsClient::RecordURLAccess(const std::string& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // Move the URL to the front of the cache.
  recent_urls_.remove(url);
  recent_urls_.push_front(url);
}

void WebRestrictionsClient::UpdateCache(const std::string& provider_authority,
                                        const std::string& url,
                                        ScopedJavaGlobalRef<jobject> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // If the webrestrictions provider changed when the old one was being queried,
  // do not update the cache for the new provider.
  if (provider_authority != provider_authority_)
    return;
  RecordURLAccess(url);
  if (recent_urls_.size() >= kMaxCacheSize) {
    cache_.RemoveCacheEntry(recent_urls_.back());
    recent_urls_.pop_back();
  }
  cache_.SetCacheEntry(url, WebRestrictionsClientResult(result));
}

void WebRestrictionsClient::RequestSupportKnown(
    const std::string& provider_authority,
    bool supports_request) {
  // |supports_request_| is initialized to false.
  DCHECK(!supports_request_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // If the webrestrictions provider changed when the old one was being queried,
  // ignore the result.
  if (provider_authority != provider_authority_)
    return;
  supports_request_ = supports_request;
}

void WebRestrictionsClient::OnShouldProceedComplete(
    std::string provider_authority,
    const std::string& url,
    const base::Callback<void(bool)>& callback,
    const ScopedJavaGlobalRef<jobject>& result) {
  UpdateCache(provider_authority, url, result);
  callback.Run(cache_.GetCacheEntry(url)->ShouldProceed());
}

void WebRestrictionsClient::ClearCache() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  cache_.Clear();
  recent_urls_.clear();
}

std::unique_ptr<WebRestrictionsClientResult>
WebRestrictionsClient::GetCachedWebRestrictionsResult(const std::string& url) {
  return cache_.GetCacheEntry(url);
}

// static
ScopedJavaGlobalRef<jobject> WebRestrictionsClient::ShouldProceedTask(
    const std::string& url,
    const base::android::JavaRef<jobject>& java_provider) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaGlobalRef<jobject> result(
      Java_WebRestrictionsClient_shouldProceed(
          env, java_provider,
          base::android::ConvertUTF8ToJavaString(env, url)));
  return result;
}

WebRestrictionsClient::Cache::Cache() = default;

WebRestrictionsClient::Cache::~Cache() = default;

std::unique_ptr<WebRestrictionsClientResult>
WebRestrictionsClient::Cache::GetCacheEntry(const std::string& url) {
  base::AutoLock lock(lock_);
  auto iter = cache_data_.find(url);
  if (iter == cache_data_.end())
    return nullptr;
  // This has to be thread-safe, so copy the data.
  return std::unique_ptr<WebRestrictionsClientResult>(
      new WebRestrictionsClientResult(iter->second));
}

void WebRestrictionsClient::Cache::SetCacheEntry(
    const std::string& url,
    const WebRestrictionsClientResult& entry) {
  base::AutoLock lock(lock_);
  cache_data_.emplace(url, entry);
}

void WebRestrictionsClient::Cache::RemoveCacheEntry(const std::string& url) {
  base::AutoLock lock(lock_);
  cache_data_.erase(url);
}

void WebRestrictionsClient::Cache::Clear() {
  base::AutoLock lock(lock_);
  cache_data_.clear();
}

}  // namespace web_restrictions
