// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/android/translate_message.h"

#include <stdint.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "components/messages/android/message_enums.h"
#include "components/translate/content/android/jni_headers/TranslateMessage_jni.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "content/public/browser/web_contents.h"

namespace translate {

namespace {

// Default implementation of the TranslateMessage::Bridge interface, which just
// calls the appropriate Java methods in each case.
class BridgeImpl : public TranslateMessage::Bridge {
 public:
  ~BridgeImpl() override;

  void CreateTranslateMessage(JNIEnv* env,
                              content::WebContents* web_contents,
                              TranslateMessage* native_translate_message,
                              jint dismissal_duration_seconds) override {
    DCHECK(!java_translate_message_);
    java_translate_message_ = Java_TranslateMessage_create(
        env, web_contents->GetJavaWebContents(),
        reinterpret_cast<intptr_t>(native_translate_message),
        dismissal_duration_seconds);
  }

  void ShowTranslateError(JNIEnv* env,
                          content::WebContents* web_contents) override {
    Java_TranslateMessage_showTranslateError(
        env, web_contents->GetJavaWebContents());
  }

  void ShowBeforeTranslateMessage(
      JNIEnv* env,
      base::android::ScopedJavaLocalRef<jstring> source_language_display_name,
      base::android::ScopedJavaLocalRef<jstring> target_language_display_name)
      override {
    Java_TranslateMessage_showBeforeTranslateMessage(
        env, java_translate_message_, std::move(source_language_display_name),
        std::move(target_language_display_name));
  }

  void ShowTranslationInProgressMessage(
      JNIEnv* env,
      base::android::ScopedJavaLocalRef<jstring> source_language_display_name,
      base::android::ScopedJavaLocalRef<jstring> target_language_display_name)
      override {
    Java_TranslateMessage_showTranslationInProgressMessage(
        env, java_translate_message_, std::move(source_language_display_name),
        std::move(target_language_display_name));
  }

  void ShowAfterTranslateMessage(
      JNIEnv* env,
      base::android::ScopedJavaLocalRef<jstring> source_language_display_name,
      base::android::ScopedJavaLocalRef<jstring> target_language_display_name)
      override {
    Java_TranslateMessage_showAfterTranslateMessage(
        env, java_translate_message_, std::move(source_language_display_name),
        std::move(target_language_display_name));
  }

  void ClearNativePointer(JNIEnv* env) override {
    Java_TranslateMessage_clearNativePointer(env, java_translate_message_);
  }

  void Dismiss(JNIEnv* env) override {
    if (java_translate_message_)
      Java_TranslateMessage_dismiss(env, java_translate_message_);
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_translate_message_;
};

BridgeImpl::~BridgeImpl() = default;

// Returns the auto-dismiss timer duration in seconds for the translate message,
// which defaults to 10s and can be overridden by a Feature param.
int GetDismissalDurationSeconds() {
  constexpr base::FeatureParam<int> kDismissalDuration(
      &kTranslateMessageUI, "dismissal_duration_sec", 10);
  return kDismissalDuration.Get();
}

}  // namespace

const base::Feature kTranslateMessageUI("TranslateMessageUI",
                                        base::FEATURE_DISABLED_BY_DEFAULT);

TranslateMessage::Bridge::~Bridge() = default;

TranslateMessage::TranslateMessage(
    content::WebContents* web_contents,
    const base::WeakPtr<TranslateManager>& translate_manager,
    base::OnceCallback<void()> on_dismiss_callback,
    std::unique_ptr<Bridge> bridge)
    : web_contents_(web_contents),
      translate_manager_(translate_manager),
      on_dismiss_callback_(std::move(on_dismiss_callback)),
      bridge_(std::move(bridge)) {
  DCHECK(!on_dismiss_callback_.is_null());
}

TranslateMessage::TranslateMessage(
    content::WebContents* web_contents,
    const base::WeakPtr<TranslateManager>& translate_manager,
    base::OnceCallback<void()> on_dismiss_callback)
    : TranslateMessage(web_contents,
                       translate_manager,
                       std::move(on_dismiss_callback),
                       std::make_unique<BridgeImpl>()) {}

TranslateMessage::~TranslateMessage() {
  // Clear the |on_dismiss_callback_| so that it doesn't get run during object
  // destruction. This prevents a possible use-after-free if the callback points
  // to a method on the owner of |this|.
  on_dismiss_callback_.Reset();
  if (bridge_)
    bridge_->Dismiss(base::android::AttachCurrentThread());
}

void TranslateMessage::ShowTranslateStep(TranslateStep step,
                                         const std::string& source_language,
                                         const std::string& target_language) {
  // Once the TranslateMessage has been dismissed, ShowTranslateStep() should
  // not be called again on it.
  DCHECK(!on_dismiss_callback_.is_null());
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!ui_delegate_) {
    ui_delegate_ = std::make_unique<TranslateUIDelegate>(
        translate_manager_, source_language, target_language);
    bridge_->CreateTranslateMessage(env, web_contents_, this,
                                    GetDismissalDurationSeconds());
  }

  if (ui_delegate_->GetSourceLanguageCode() != source_language)
    ui_delegate_->UpdateSourceLanguage(source_language);
  if (ui_delegate_->GetTargetLanguageCode() != target_language)
    ui_delegate_->UpdateTargetLanguage(target_language);

  if (step == TRANSLATE_STEP_TRANSLATE_ERROR) {
    bridge_->ShowTranslateError(env, web_contents_);

    // Since an error occurred, show the UI in the last good state.
    const LanguageState* language_state = ui_delegate_->GetLanguageState();
    if (language_state && language_state->IsPageTranslated())
      step = TRANSLATE_STEP_AFTER_TRANSLATE;
    else
      step = TRANSLATE_STEP_BEFORE_TRANSLATE;
  }
  translate_step_ = step;

  auto source_language_display_name = base::android::ConvertUTF16ToJavaString(
      env,
      ui_delegate_->GetLanguageNameAt(ui_delegate_->GetSourceLanguageIndex()));
  auto target_language_display_name = base::android::ConvertUTF16ToJavaString(
      env,
      ui_delegate_->GetLanguageNameAt(ui_delegate_->GetTargetLanguageIndex()));

  switch (translate_step_) {
    case TRANSLATE_STEP_BEFORE_TRANSLATE:
      bridge_->ShowBeforeTranslateMessage(
          env, std::move(source_language_display_name),
          std::move(target_language_display_name));
      break;

    case TRANSLATE_STEP_TRANSLATING:
      bridge_->ShowTranslationInProgressMessage(
          env, std::move(source_language_display_name),
          std::move(target_language_display_name));
      break;

    case TRANSLATE_STEP_AFTER_TRANSLATE:
      bridge_->ShowAfterTranslateMessage(
          env, std::move(source_language_display_name),
          std::move(target_language_display_name));
      break;

    default:
      NOTREACHED();
      break;
  }
}

void TranslateMessage::HandlePrimaryAction(JNIEnv* env) {
  switch (translate_step_) {
    case TRANSLATE_STEP_BEFORE_TRANSLATE:
      ui_delegate_->ReportUIInteraction(UIInteraction::kTranslate);
      ui_delegate_->Translate();
      break;

    case TRANSLATE_STEP_AFTER_TRANSLATE:
      ui_delegate_->ReportUIInteraction(UIInteraction::kRevert);

      // The TranslateManager doesn't make another call to show the
      // pre-translation UI during RevertTranslation(), so show the correct UI
      // here. This is done before calling RevertTranslation() just in case
      // RevertTranslation() causes this message to be dismissed or destroyed,
      // since that could cause a use-after-free.
      ShowTranslateStep(TRANSLATE_STEP_BEFORE_TRANSLATE,
                        ui_delegate_->GetSourceLanguageCode(),
                        ui_delegate_->GetTargetLanguageCode());

      ui_delegate_->RevertTranslation();
      break;

    case TRANSLATE_STEP_TRANSLATING:
      // TODO(crbug.com/1304118): Once the TRANSLATE_STEP_TRANSLATING state
      // replaces the primary action button with a spinning progress indicator
      // (and thus there is no longer a clickable primary button), fall through
      // to the |default| NOTREACHED handler below. Until then, do nothing in
      // this case.
      break;

    default:
      NOTREACHED();
      break;
  }
}

void TranslateMessage::HandleDismiss(JNIEnv* env, jint dismiss_reason) {
  switch (static_cast<messages::DismissReason>(dismiss_reason)) {
    case messages::DismissReason::PRIMARY_ACTION:
    case messages::DismissReason::SECONDARY_ACTION:
    case messages::DismissReason::GESTURE:
      ui_delegate_->ReportUIInteraction(UIInteraction::kCloseUIExplicitly);
      ui_delegate_->OnUIClosedByUser();
      break;

    case messages::DismissReason::TAB_SWITCHED:
    case messages::DismissReason::TAB_DESTROYED:
    case messages::DismissReason::ACTIVITY_DESTROYED:
    case messages::DismissReason::SCOPE_DESTROYED:
      ui_delegate_->ReportUIInteraction(UIInteraction::kCloseUILostFocus);
      break;

    case messages::DismissReason::TIMER:
      ui_delegate_->ReportUIInteraction(UIInteraction::kCloseUITimerRanOut);
      break;

    default:
      break;
  }

  bridge_->ClearNativePointer(env);
  bridge_.reset();

  // The only time |on_dismiss_callback_| will be null is during the destruction
  // of |this|.
  if (!on_dismiss_callback_.is_null()) {
    // Note that this callback can destroy |this|, so this method shouldn't do
    // anything afterwards.
    std::move(on_dismiss_callback_).Run();
  }
}

}  // namespace translate
