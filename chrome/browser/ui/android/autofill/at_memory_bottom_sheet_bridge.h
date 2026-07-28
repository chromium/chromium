// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AT_MEMORY_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AT_MEMORY_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"

class Profile;

namespace ui {
class WindowAndroid;
}

namespace autofill {

class AtMemorySuggestionController;

// Bridge class owned by `AtMemorySuggestionController` providing an entry point
// to trigger the @memory bottom sheet on Android.
class AtMemoryBottomSheetBridge {
 public:
  AtMemoryBottomSheetBridge(ui::WindowAndroid* window_android,
                            Profile* profile,
                            AtMemorySuggestionController* controller);

  AtMemoryBottomSheetBridge(const AtMemoryBottomSheetBridge&) = delete;
  AtMemoryBottomSheetBridge& operator=(const AtMemoryBottomSheetBridge&) =
      delete;

  virtual ~AtMemoryBottomSheetBridge();

 protected:
  explicit AtMemoryBottomSheetBridge(AtMemorySuggestionController* controller);

 public:
  // Requests to show the bottom sheet.
  virtual void RequestShowContent(base::span<const Suggestion> suggestions);

  // Requests to hide the bottom sheet.
  void Hide();

  // -- JNI calls bridged to AtMemorySuggestionController --
  void OnDismissed(JNIEnv* env);
  void OnQuerySubmitted(JNIEnv* env, const std::u16string& query);
  void OnQueryTextChanged(JNIEnv* env, const std::u16string& query);
  void OnSuggestionSelected(JNIEnv* env, int position);
  void OnSuggestionDismissed(JNIEnv* env, int position);
  void OnChildSuggestionsShown(JNIEnv* env, int parent_position);
  void OnChildSuggestionSelected(JNIEnv* env,
                                 int parent_position,
                                 int child_position);
  bool IsSearching(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  const raw_ref<AtMemorySuggestionController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AT_MEMORY_BOTTOM_SHEET_BRIDGE_H_
