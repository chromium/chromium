// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_LOGO_LOGO_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_LOGO_LOGO_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace search_provider_logos {
class LogoService;
}  // namespace search_provider_logos

// The C++ counterpart to LogoBridge.java. Enables Java code to access the
// default search provider's logo.
class LogoBridge {
 public:
  explicit LogoBridge(Profile* profile);

  LogoBridge(const LogoBridge&) = delete;
  LogoBridge& operator=(const LogoBridge&) = delete;

  void Destroy(JNIEnv* env);

  // Gets the current non-animated logo (downloading it if necessary) and passes
  // it to the observer.
  // The observer's |onLogoAvailable| is guaranteed to be called at least once:
  // a) A cached doodle is available.
  // b) A new doodle is available.
  // c) Not having a doodle was revalidated.
  void GetCurrentLogo(JNIEnv* env,
                      const base::android::JavaRef<jobject>& j_logo_observer);

  // Fires a fire-and-forget HTTP GET request to the specified |log_url| to
  // record a Doodle impression on the server.
  void RecordImpression(JNIEnv* env, std::string_view log_url);

 private:
  virtual ~LogoBridge();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<search_provider_logos::LogoService> logo_service_;

  base::WeakPtrFactory<LogoBridge> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ANDROID_LOGO_LOGO_BRIDGE_H_
