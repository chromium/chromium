// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spellchecker_session_bridge_android.h"

#include <stddef.h>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/spellcheck/browser/android/jni_headers/SpellCheckerSessionBridge_jni.h"

using base::android::JavaParamRef;

SpellCheckerSessionBridge::SpellCheckerSessionBridge()
    : java_object_initialization_failed_(false) {}

SpellCheckerSessionBridge::~SpellCheckerSessionBridge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Clean-up java side to avoid any stale JNI callbacks.
  DisconnectSession();
}

void SpellCheckerSessionBridge::RequestTextCheck(
    const std::u16string& text,
    RequestTextCheckCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This allows us to discard |callback| safely in case it's not run due to
  // failures in initialization of |java_object_|.
  std::unique_ptr<SpellingRequest> incoming_request =
      std::make_unique<SpellingRequest>(text, std::move(callback));

  // SpellCheckerSessionBridge#create() will return null if spell checker
  // service is unavailable.
  if (java_object_initialization_failed_) {
    return;
  }

  // RequestTextCheck API call arrives at the SpellCheckHost before
  // DisconnectSessionBridge when the user focuses an input field that already
  // contains completed text.  We need to initialize the spellchecker here
  // rather than in response to DisconnectSessionBridge so that the existing
  // text will be spellchecked immediately.
  if (java_object_.is_null()) {
    java_object_.Reset(Java_SpellCheckerSessionBridge_create(
        base::android::AttachCurrentThread(),
        reinterpret_cast<intptr_t>(this)));
    if (java_object_.is_null()) {
      java_object_initialization_failed_ = true;
      return;
    }
  }

  // Save incoming requests to run at the end of the currently active request.
  // If multiple requests arrive during one active request, only the most
  // recent request will run (the others get overwritten).
  if (active_request_) {
    pending_request_ = std::move(incoming_request);
    return;
  }

  active_request_ = std::move(incoming_request);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SpellCheckerSessionBridge_requestTextCheck(
      env, java_object_, base::android::ConvertUTF16ToJavaString(env, text));
}

void SpellCheckerSessionBridge::ProcessSpellCheckResults(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    const JavaParamRef<jintArray>& offset_array,
    const JavaParamRef<jintArray>& length_array,
    const JavaParamRef<jobjectArray>& suggestions_array) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::vector<int> offsets;
  std::vector<int> lengths;

  base::android::JavaIntArrayToIntVector(env, offset_array, &offsets);
  base::android::JavaIntArrayToIntVector(env, length_array, &lengths);

  std::vector<SpellCheckResult> results;
  for (size_t i = 0; i < offsets.size(); i++) {
    base::android::ScopedJavaLocalRef<jobjectArray> suggestions_for_word_array(
        env, static_cast<jobjectArray>(
                 env->GetObjectArrayElement(suggestions_array, i)));
    std::vector<std::u16string> suggestions_for_word;
    base::android::AppendJavaStringArrayToStringVector(
        env, suggestions_for_word_array, &suggestions_for_word);
    results.push_back(SpellCheckResult(SpellCheckResult::SPELLING, offsets[i],
                                       lengths[i], suggestions_for_word));
  }

  std::move(active_request_->callback_).Run(results);

  active_request_ = std::move(pending_request_);
  if (active_request_) {
    Java_SpellCheckerSessionBridge_requestTextCheck(
        env, java_object_,
        base::android::ConvertUTF16ToJavaString(env, active_request_->text_));
  }
}

void SpellCheckerSessionBridge::DisconnectSession() {
  // Needs to be executed on the same thread as the RequestTextCheck and
  // ProcessSpellCheckResults methods, which is the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  active_request_.reset();
  pending_request_.reset();

  if (!java_object_.is_null()) {
    Java_SpellCheckerSessionBridge_disconnect(
        base::android::AttachCurrentThread(), java_object_);
    java_object_.Reset();
  }
}

SpellCheckerSessionBridge::SpellingRequest::SpellingRequest(
    const std::u16string& text,
    RequestTextCheckCallback callback)
    : text_(text), callback_(std::move(callback)) {}

SpellCheckerSessionBridge::SpellingRequest::~SpellingRequest() {
  // Ensure that we don't clear an uncalled RequestTextCheckCallback
  if (callback_)
    std::move(callback_).Run(std::vector<SpellCheckResult>());
}
