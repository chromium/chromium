// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/android/media_router_android_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/media_router/browser/android/flinging_controller_bridge.h"
#include "components/media_router/browser/android/media_router_android.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_controller.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/media_router/browser/android/jni_headers/BrowserMediaRouter_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace media_router {

MediaRouterAndroidBridge::MediaRouterAndroidBridge(MediaRouterAndroid* router)
    : native_media_router_(router) {
  JNIEnv* env = AttachCurrentThread();
  java_media_router_.Reset(
      Java_BrowserMediaRouter_create(env, reinterpret_cast<int64_t>(this)));
}

MediaRouterAndroidBridge::~MediaRouterAndroidBridge() {
  JNIEnv* env = AttachCurrentThread();
  // When |this| is destroyed, there might still pending runnables on the Java
  // side, that are keeping the Java object alive. These runnables might try to
  // call back to the native side when executed. We need to signal to the Java
  // counterpart that it can't call back into native anymore.
  Java_BrowserMediaRouter_teardown(env, java_media_router_);
}

void MediaRouterAndroidBridge::CreateRoute(const MediaSource::Id& source_id,
                                           const MediaSink::Id& sink_id,
                                           const std::string& presentation_id,
                                           const url::Origin& origin,
                                           content::WebContents* web_contents,
                                           int route_request_id) {
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_web_contents;
  if (web_contents) {
    java_web_contents = web_contents->GetJavaWebContents();
  }

  Java_BrowserMediaRouter_createRoute(
      env, java_media_router_, source_id, sink_id, presentation_id,
      origin.GetURL().spec(), java_web_contents, route_request_id);
}

void MediaRouterAndroidBridge::JoinRoute(const MediaSource::Id& source_id,
                                         const std::string& presentation_id,
                                         const url::Origin& origin,
                                         content::WebContents* web_contents,
                                         int route_request_id) {
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_web_contents;
  if (web_contents) {
    java_web_contents = web_contents->GetJavaWebContents();
  }

  Java_BrowserMediaRouter_joinRoute(env, java_media_router_, source_id,
                                    presentation_id, origin.GetURL().spec(),
                                    java_web_contents, route_request_id);
}

void MediaRouterAndroidBridge::TerminateRoute(const MediaRoute::Id& route_id) {
  JNIEnv* env = AttachCurrentThread();
  Java_BrowserMediaRouter_closeRoute(env, java_media_router_, route_id);
}

void MediaRouterAndroidBridge::SendRouteMessage(const MediaRoute::Id& route_id,
                                                const std::string& message) {
  JNIEnv* env = AttachCurrentThread();
  Java_BrowserMediaRouter_sendStringMessage(env, java_media_router_, route_id,
                                            message);
}

void MediaRouterAndroidBridge::DetachRoute(const MediaRoute::Id& route_id) {
  JNIEnv* env = AttachCurrentThread();
  Java_BrowserMediaRouter_detachRoute(env, java_media_router_, route_id);
}

bool MediaRouterAndroidBridge::StartObservingMediaSinks(
    const MediaSource::Id& source_id,
    const url::Origin& origin) {
  JNIEnv* env = AttachCurrentThread();
  return Java_BrowserMediaRouter_startObservingMediaSinks(
      env, java_media_router_, source_id, origin.GetURL().spec());
}

void MediaRouterAndroidBridge::StopObservingMediaSinks(
    const MediaSource::Id& source_id) {
  JNIEnv* env = AttachCurrentThread();
  Java_BrowserMediaRouter_stopObservingMediaSinks(env, java_media_router_,
                                                  source_id);
}

std::unique_ptr<media::FlingingController>
MediaRouterAndroidBridge::GetFlingingController(
    const MediaRoute::Id& route_id) {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaGlobalRef<jobject> flinging_controller;

  flinging_controller.Reset(Java_BrowserMediaRouter_getFlingingControllerBridge(
      env, java_media_router_, route_id));

  if (flinging_controller.is_null()) {
    return nullptr;
  }

  return std::make_unique<FlingingControllerBridge>(flinging_controller);
}

void MediaRouterAndroidBridge::OnSinksReceived(JNIEnv* env,
                                               const std::string& source_urn,
                                               int32_t count) {
  std::vector<MediaSink> sinks_converted;
  sinks_converted.reserve(count);
  for (int i = 0; i < count; ++i) {
    sinks_converted.push_back(MediaSink(
        Java_BrowserMediaRouter_getSinkUrn(env, java_media_router_, source_urn,
                                           i),
        Java_BrowserMediaRouter_getSinkName(env, java_media_router_, source_urn,
                                            i),
        SinkIconType::GENERIC, mojom::MediaRouteProviderId::ANDROID_CAF));
  }
  native_media_router_->OnSinksReceived(source_urn, sinks_converted);
}

void MediaRouterAndroidBridge::OnRouteCreated(JNIEnv* env,
                                              const std::string& media_route_id,
                                              const std::string& sink_id,
                                              int32_t route_request_id,
                                              bool is_local) {
  native_media_router_->OnRouteCreated(media_route_id, sink_id,
                                       route_request_id, is_local);
}

void MediaRouterAndroidBridge::OnRouteMediaSourceUpdated(
    JNIEnv* env,
    const std::string& media_route_id,
    const std::string& media_source_id) {
  native_media_router_->OnRouteMediaSourceUpdated(media_route_id,
                                                  media_source_id);
}

void MediaRouterAndroidBridge::OnCreateRouteRequestError(
    JNIEnv* env,
    const std::string& error_text,
    int32_t route_request_id) {
  native_media_router_->OnCreateRouteRequestError(error_text, route_request_id);
}

void MediaRouterAndroidBridge::OnJoinRouteRequestError(
    JNIEnv* env,
    const std::string& error_text,
    int32_t route_request_id) {
  native_media_router_->OnJoinRouteRequestError(error_text, route_request_id);
}

void MediaRouterAndroidBridge::OnRouteTerminated(
    JNIEnv* env,
    const std::string& media_route_id) {
  native_media_router_->OnRouteTerminated(media_route_id);
}

void MediaRouterAndroidBridge::OnRouteClosed(
    JNIEnv* env,
    const std::string& media_route_id,
    const std::optional<std::string>& error) {
  native_media_router_->OnRouteClosed(media_route_id, error);
}

void MediaRouterAndroidBridge::OnMessage(JNIEnv* env,
                                         const std::string& media_route_id,
                                         const std::string& message) {
  native_media_router_->OnMessage(media_route_id, message);
}

}  // namespace media_router

DEFINE_JNI(BrowserMediaRouter)
