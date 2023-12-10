// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/android/webapk/webapk_database.h"
#include "chrome/browser/android/webapk/webapk_database_factory.h"
#include "chrome/browser/android/webapk/webapk_registrar.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/webapps/android/webapps_jni_headers/PwaRestorePromoUtils_jni.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_type_store.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {

class HandleWebApkDatabaseRequest {
 public:
  HandleWebApkDatabaseRequest(JNIEnv* env,
                              Profile* profile,
                              ui::WindowAndroid* window_android,
                              int arrow_resource_id) {
    env_ = env;
    window_android_ = window_android;
    arrow_resource_id_ = arrow_resource_id;

    database_factory_ =
        std::make_unique<webapk::WebApkDatabaseFactory>(profile);

    web_apk_database_ = std::make_unique<webapk::WebApkDatabase>(
        database_factory_.get(),
        base::BindRepeating(&HandleWebApkDatabaseRequest::ErrorCallback,
                            base::Unretained(this)));

    web_apk_database_->OpenDatabase(base::BindRepeating(
        &HandleWebApkDatabaseRequest::DatabaseOpened, base::Unretained(this)));
  }

 private:
  void ReturnResultsAndDie(bool success) {
    ScopedJavaLocalRef<jobjectArray> jresults =
        base::android::ToJavaArrayOfStringArray(env_, results_);
    webapps::Java_PwaRestorePromoUtils_onRestorableAppsAvailable(
        env_, success, jresults, window_android_->GetJavaObject(),
        arrow_resource_id_);
    delete this;
  }

  void DatabaseOpened(webapk::Registry registry,
                      std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
    // The registry is a map of webapps::AppId -> std::unique_ptr<WebApkProto>.
    for (auto const& [appId, proto] : registry) {
      LOG(WARNING) << "Found app id " << appId;
    }

    // TODO(finnur): Stop hardcoding the apps and use for loop above.
    results_ = {{"foo", "Bar"}, {"bar", "Foo"}, {"foobar", "Barfoo"}};

    ReturnResultsAndDie(true);
  }

  void ErrorCallback(const syncer::ModelError& error) {
    ReturnResultsAndDie(false);
  }

  raw_ptr<JNIEnv> env_ = nullptr;
  raw_ptr<ui::WindowAndroid> window_android_ = nullptr;
  int arrow_resource_id_ = 0;

  std::unique_ptr<webapk::WebApkDatabaseFactory> database_factory_;
  std::unique_ptr<webapk::WebApkDatabase> web_apk_database_;

  std::vector<std::vector<std::string>> results_;
};

}  // namespace

namespace webapps {

// static
void JNI_PwaRestorePromoUtils_FetchRestorableApps(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile,
    const JavaParamRef<jobject>& jwindow_android,
    int arrowResourceId) {
  Profile* profile =
      ProfileAndroid::FromProfileAndroid(jprofile)->GetWeakPtr().get();
  ui::WindowAndroid* window_android =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);

  // This object handles its own lifetime.
  new HandleWebApkDatabaseRequest(env, profile, window_android,
                                  arrowResourceId);
}

}  // namespace webapps
