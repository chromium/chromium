// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_ENGAGEMENT_CONTENT_ANDROID_SITE_ENGAGEMENT_SERVICE_ANDROID_H_
#define COMPONENTS_SITE_ENGAGEMENT_CONTENT_ANDROID_SITE_ENGAGEMENT_SERVICE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

namespace site_engagement {

class SiteEngagementService;

// Wrapper class to expose the Site Engagement Service to Java. This object is
// owned by the |service_| which it wraps, and is lazily created when a
// Java-side SiteEngagementService is constructed. Once created, all future
// Java-side requests for a SiteEngagementService will use the same native
// object.
//
// This class may only be used on the UI thread.
class SiteEngagementServiceAndroid {
 public:
  // Returns the Java-side SiteEngagementService object corresponding to
  // |service|.
  static const base::android::ScopedJavaGlobalRef<jobject>& GetOrCreate(
      JNIEnv* env,
      SiteEngagementService* service);

  SiteEngagementServiceAndroid(JNIEnv* env, SiteEngagementService* service);
  SiteEngagementServiceAndroid(const SiteEngagementServiceAndroid&) = delete;
  SiteEngagementServiceAndroid& operator=(
      const SiteEngagementServiceAndroid& other) = delete;

  ~SiteEngagementServiceAndroid();

  double GetScore(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& caller,
                  const base::android::JavaParamRef<jstring>& jurl) const;

  void ResetBaseScoreForURL(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& caller,
                            const base::android::JavaParamRef<jstring>& jurl,
                            double score);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_service_;
  raw_ptr<SiteEngagementService> service_;
};

}  // namespace site_engagement

#endif  // COMPONENTS_SITE_ENGAGEMENT_CONTENT_ANDROID_SITE_ENGAGEMENT_SERVICE_ANDROID_H_
