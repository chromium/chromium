// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_ANDROID_TRANSLATE_MESSAGE_H_
#define COMPONENTS_TRANSLATE_CONTENT_ANDROID_TRANSLATE_MESSAGE_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/translate/core/browser/translate_step.h"

namespace content {
class WebContents;
}

namespace translate {

class TranslateManager;
class TranslateUIDelegate;
class TranslateUILanguagesManager;

class TranslateMessage {
 public:
  TranslateMessage(content::WebContents* web_contents,
                   const base::WeakPtr<TranslateManager>& translate_manager,
                   base::RepeatingCallback<void()> on_dismiss_callback);

  TranslateMessage(const TranslateMessage&) = delete;
  TranslateMessage& operator=(const TranslateMessage&) = delete;

  // Dismiss message on destruction if it is shown.
  ~TranslateMessage();

  // Show the Translate message UI in the specified state, or update an
  // existing visible message to have the specified state if one is already
  // visible.
  void ShowTranslateStep(TranslateStep step,
                         const std::string& source_language,
                         const std::string& target_language);

  // Called by Java in response to the user clicking the primary button.
  void HandlePrimaryAction(JNIEnv* env);
  // Called by Java in response to the message being dismissed.
  void HandleDismiss(JNIEnv* env, jint dismiss_reason);

  // Called by Java in order to build the secondary overflow menu.
  base::android::ScopedJavaLocalRef<jobjectArray> BuildOverflowMenu(
      JNIEnv* env);

  // Called by Java in response to a secondary menu item being clicked. A
  // non-null return value means that another secondary menu with the returned
  // list of menu items should be shown immediately, e.g. in order to show a
  // language picker menu. The |overflow_menu_item_id| will indicate which
  // overflow menu item that this click is related to, and the |language_code|
  // indicates which language was clicked in a language picker menu (or the
  // empty string if this was the overflow menu).
  //
  // For example, if the user clicks "More languages" on the overflow menu, then
  // |overflow_menu_item_id| would be kChangeTargetLanguage, and |language_code|
  // would be an empty string. If the user clicks "French" on the "More
  // languages" language picker menu, then |overflow_menu_item_id| would be
  // kChangeTargetLanguage and |language_code| would be "fr".
  base::android::ScopedJavaLocalRef<jobjectArray>
  HandleSecondaryMenuItemClicked(
      JNIEnv* env,
      jint overflow_menu_item_id,
      const base::android::JavaRef<jstring>& language_code,
      jboolean had_checkmark);

  // Passes on JNI calls to the stored Java TranslateMessage object, if
  // applicable. This interface exists in order to make it easier to test
  // TranslateMessage.
  class Bridge {
   public:
    virtual ~Bridge();

    // A raw content::WebContents pointer is passed in instead of the Java
    // WebContents object in order to make testing easier, so that tests can
    // just use nullptr as the content::WebContents.
    virtual bool CreateTranslateMessage(
        JNIEnv* env,
        content::WebContents* web_contents,
        TranslateMessage* native_translate_message,
        jint dismissal_duration_seconds) = 0;

    virtual void ShowTranslateError(JNIEnv* env,
                                    content::WebContents* web_contents) = 0;

    virtual void ShowMessage(
        JNIEnv* env,
        base::android::ScopedJavaLocalRef<jstring> title,
        base::android::ScopedJavaLocalRef<jstring> description,
        base::android::ScopedJavaLocalRef<jstring> primary_button_text,
        jboolean has_overflow_menu) = 0;

    virtual base::android::ScopedJavaLocalRef<jobjectArray>
    ConstructMenuItemArray(
        JNIEnv* env,
        base::android::ScopedJavaLocalRef<jobjectArray> titles,
        base::android::ScopedJavaLocalRef<jobjectArray> subtitles,
        base::android::ScopedJavaLocalRef<jbooleanArray> has_checkmarks,
        base::android::ScopedJavaLocalRef<jintArray> overflow_menu_item_ids,
        base::android::ScopedJavaLocalRef<jobjectArray> language_codes) = 0;

    virtual void ClearNativePointer(JNIEnv* env) = 0;
    virtual void Dismiss(JNIEnv* env) = 0;
  };

  // Test-only constructor with a custom JavaMethodCaller.
  TranslateMessage(content::WebContents* web_contents,
                   const base::WeakPtr<TranslateManager>& translate_manager,
                   base::RepeatingCallback<void()> on_dismiss_callback,
                   std::unique_ptr<Bridge> bridge);

  // This enum is only visible for testing purposes - the Java TranslateMessage
  // treats these ids as opaque integers.
  enum class OverflowMenuItemId {
    kInvalid,
    kChangeSourceLanguage,
    kChangeTargetLanguage,
    kToggleAlwaysTranslateLanguage,
    kToggleNeverTranslateLanguage,
    kToggleNeverTranslateSite,
  };

 private:
  enum class State {
    kDismissed,

    kBeforeTranslate,
    kTranslating,
    kAfterTranslate,

    kAfterTranslateWithAutoAlwaysConfirmation,
    kAutoNeverTranslateConfirmation,
  };

  void RevertTranslationAndUpdateMessage();

  base::android::ScopedJavaLocalRef<jobjectArray> ConstructLanguagePickerMenu(
      JNIEnv* env,
      OverflowMenuItemId overflow_menu_item_id,
      base::span<const std::string> content_language_codes,
      base::span<const std::string> skip_language_codes) const;

  raw_ptr<content::WebContents> web_contents_;
  base::WeakPtr<TranslateManager> translate_manager_;
  base::RepeatingCallback<void()> on_dismiss_callback_;

  std::unique_ptr<Bridge> bridge_;

  // Constructed the first time ShowTranslateStep is called.
  std::unique_ptr<TranslateUIDelegate> ui_delegate_;
  raw_ptr<TranslateUILanguagesManager> ui_languages_manager_;
  State state_ = State::kDismissed;

  // Keeps track of whether or not this TranslateMessage has ever been
  // interacted with in any way aside from dismissing it.
  bool has_been_interacted_with_ = false;

  // Keeps track of whether or not a translation is in progress that could
  // trigger automatically setting "always translate language".
  bool is_translation_eligible_for_auto_always_translate_ = false;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_ANDROID_TRANSLATE_MESSAGE_H_
