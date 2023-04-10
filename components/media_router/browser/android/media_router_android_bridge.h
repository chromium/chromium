// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_BROWSER_ANDROID_MEDIA_ROUTER_ANDROID_BRIDGE_H_
#define COMPONENTS_MEDIA_ROUTER_BROWSER_ANDROID_MEDIA_ROUTER_ANDROID_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "media/base/flinging_controller.h"
#include "url/origin.h"

namespace content {
class WebContents;
}

namespace media_router {

class MediaRouterAndroid;

// Wraps the JNI layer between MediaRouterAndroid and ChromeMediaRouter.
class MediaRouterAndroidBridge {
 public:
  explicit MediaRouterAndroidBridge(MediaRouterAndroid* router);

  MediaRouterAndroidBridge(const MediaRouterAndroidBridge&) = delete;
  MediaRouterAndroidBridge& operator=(const MediaRouterAndroidBridge&) = delete;

  virtual ~MediaRouterAndroidBridge();

  // Implement the corresponding calls for the MediaRouterAndroid class.
  // Virtual so could be overridden by tests.
  virtual void CreateRoute(const MediaSource::Id& source_id,
                           const MediaSink::Id& sink_id,
                           const std::string& presentation_id,
                           const url::Origin& origin,
                           content::WebContents* web_contents,
                           int route_request_id);
  virtual void JoinRoute(const MediaSource::Id& source_id,
                         const std::string& presentation_id,
                         const url::Origin& origin,
                         content::WebContents* web_contents,
                         int route_request_id);
  virtual void TerminateRoute(const MediaRoute::Id& route_id);
  virtual void SendRouteMessage(const MediaRoute::Id& route_id,
                                const std::string& message);
  virtual void DetachRoute(const MediaRoute::Id& route_id);
  virtual bool StartObservingMediaSinks(const MediaSource::Id& source_id);
  virtual void StopObservingMediaSinks(const MediaSource::Id& source_id);
  virtual std::unique_ptr<media::FlingingController> GetFlingingController(
      const MediaRoute::Id& route_id);

  // Methods called by the Java counterpart.
  void OnSinksReceived(JNIEnv* env,
                       const base::android::JavaRef<jobject>& obj,
                       const base::android::JavaRef<jstring>& jsource_urn,
                       jint jcount);
  void OnRouteCreated(JNIEnv* env,
                      const base::android::JavaRef<jobject>& obj,
                      const base::android::JavaRef<jstring>& jmedia_route_id,
                      const base::android::JavaRef<jstring>& jmedia_sink_id,
                      jint jroute_request_id,
                      jboolean jis_local);
  void OnCreateRouteRequestError(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj,
      const base::android::JavaRef<jstring>& jerror_text,
      jint jroute_request_id);
  void OnJoinRouteRequestError(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj,
      const base::android::JavaRef<jstring>& jerror_text,
      jint jroute_request_id);
  void OnRouteTerminated(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj,
      const base::android::JavaRef<jstring>& jmedia_route_id);
  void OnRouteClosed(JNIEnv* env,
                     const base::android::JavaRef<jobject>& obj,
                     const base::android::JavaRef<jstring>& jmedia_route_id,
                     const base::android::JavaRef<jstring>& jerror);
  void OnMessage(JNIEnv* env,
                 const base::android::JavaRef<jobject>& obj,
                 const base::android::JavaRef<jstring>& jmedia_route_id,
                 const base::android::JavaRef<jstring>& jmessage);
  void OnRouteMediaSourceUpdated(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj,
      const base::android::JavaRef<jstring>& jmedia_route_id,
      const base::android::JavaRef<jstring>& jmedia_source_id);

 private:
  raw_ptr<MediaRouterAndroid> native_media_router_;
  base::android::ScopedJavaGlobalRef<jobject> java_media_router_;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_BROWSER_ANDROID_MEDIA_ROUTER_ANDROID_BRIDGE_H_
