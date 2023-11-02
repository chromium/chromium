// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECKER_SESSION_BRIDGE_ANDROID_H_
#define COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECKER_SESSION_BRIDGE_ANDROID_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "components/spellcheck/common/spellcheck.mojom.h"

// A class used to interface between the Java class of the same name and the
// android SpellCheckHost.  This class receives text to be spellchecked, sends
// that text to the Java side via JNI to be spellchecked, and then sends those
// results to the renderer.
class SpellCheckerSessionBridge {
 public:
  SpellCheckerSessionBridge();

  SpellCheckerSessionBridge(const SpellCheckerSessionBridge&) = delete;
  SpellCheckerSessionBridge& operator=(const SpellCheckerSessionBridge&) =
      delete;

  ~SpellCheckerSessionBridge();

  using RequestTextCheckCallback =
      spellcheck::mojom::SpellCheckHost::RequestTextCheckCallback;

  // Receives text to be checked and sends it to Java to be spellchecked.
  void RequestTextCheck(const std::u16string& text,
                        RequestTextCheckCallback callback);

  // Receives information from Java side about the typos in a given string
  // of text, processes these and sends them to the renderer.
  void ProcessSpellCheckResults(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jintArray>& offset_array,
      const base::android::JavaParamRef<jintArray>& length_array,
      const base::android::JavaParamRef<jobjectArray>& suggestions_array);

  // Sets the handle to the Java SpellCheckerSessionBridge object to null,
  // marking the Java object for garbage collection.
  void DisconnectSession();

 private:
  class SpellingRequest {
   public:
    SpellingRequest(const std::u16string& text,
                    RequestTextCheckCallback callback);

    SpellingRequest(const SpellingRequest&) = delete;
    SpellingRequest& operator=(const SpellingRequest&) = delete;

    ~SpellingRequest();

    std::u16string text_;
    RequestTextCheckCallback callback_;
  };

  std::unique_ptr<SpellingRequest> active_request_;
  std::unique_ptr<SpellingRequest> pending_request_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  bool java_object_initialization_failed_;
};

#endif  // COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECKER_SESSION_BRIDGE_ANDROID_H_
