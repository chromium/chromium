// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/android/translate_message.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "components/messages/android/message_enums.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/content/android/jni_headers/TranslateMessage_jni.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_client.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_driver.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/translate_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

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

  void ShowMessage(
      JNIEnv* env,
      base::android::ScopedJavaLocalRef<jstring> title,
      base::android::ScopedJavaLocalRef<jstring> description,
      base::android::ScopedJavaLocalRef<jstring> primary_button_text) override {
    Java_TranslateMessage_showMessage(env, java_translate_message_,
                                      std::move(title), std::move(description),
                                      std::move(primary_button_text));
  }

  base::android::ScopedJavaLocalRef<jobjectArray> ConstructMenuItemArray(
      JNIEnv* env,
      base::android::ScopedJavaLocalRef<jobjectArray> titles,
      base::android::ScopedJavaLocalRef<jobjectArray> subtitles,
      base::android::ScopedJavaLocalRef<jbooleanArray> has_checkmarks,
      base::android::ScopedJavaLocalRef<jintArray> overflow_menu_item_ids,
      base::android::ScopedJavaLocalRef<jobjectArray> language_codes) override {
    return Java_TranslateMessage_constructMenuItemArray(
        env, std::move(titles), std::move(subtitles), std::move(has_checkmarks),
        std::move(overflow_menu_item_ids), std::move(language_codes));
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

  base::android::ScopedJavaLocalRef<jstring> title;
  base::android::ScopedJavaLocalRef<jstring> primary_button_text;

  switch (translate_step_) {
    case TRANSLATE_STEP_BEFORE_TRANSLATE:
      title = base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(
                   IDS_TRANSLATE_MESSAGE_BEFORE_TRANSLATE_TITLE));
      primary_button_text = base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUTTON));
      break;

    case TRANSLATE_STEP_TRANSLATING:
      title = base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(
                   IDS_TRANSLATE_MESSAGE_BEFORE_TRANSLATE_TITLE));
      primary_button_text = nullptr;
      break;

    case TRANSLATE_STEP_AFTER_TRANSLATE:
      title = base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(
                   IDS_TRANSLATE_MESSAGE_AFTER_TRANSLATE_TITLE));
      primary_button_text = base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(IDS_TRANSLATE_MESSAGE_UNDO_BUTTON));
      break;

    default:
      NOTREACHED();
      break;
  }

  const std::u16string& source_language_display_name =
      ui_delegate_->GetLanguageNameAt(ui_delegate_->GetSourceLanguageIndex());
  const std::u16string& target_language_display_name =
      ui_delegate_->GetLanguageNameAt(ui_delegate_->GetTargetLanguageIndex());
  base::android::ScopedJavaLocalRef<jstring> description =
      base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringFUTF16(IDS_TRANSLATE_MESSAGE_DESCRIPTION,
                                          source_language_display_name,
                                          target_language_display_name));

  bridge_->ShowMessage(env, std::move(title), std::move(description),
                       std::move(primary_button_text));
}

void TranslateMessage::HandlePrimaryAction(JNIEnv* env) {
  switch (translate_step_) {
    case TRANSLATE_STEP_BEFORE_TRANSLATE:
      ui_delegate_->ReportUIInteraction(UIInteraction::kTranslate);
      ui_delegate_->Translate();
      break;

    case TRANSLATE_STEP_AFTER_TRANSLATE:
      ui_delegate_->ReportUIInteraction(UIInteraction::kRevert);
      RevertTranslationAndUpdateMessage();
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

base::android::ScopedJavaLocalRef<jobjectArray>
TranslateMessage::BuildOverflowMenu(JNIEnv* env) {
  // |titles| must have the capacity to fit the maximum number of menu items in
  // the overflow menu, including dividers.
  std::u16string titles[1U +  // Change target language.
                        1U +  // Divider.
                        1U +  // Always translate language.
                        1U +  // Never translate language.
                        1U +  // Never translate site.
                        1U];  // Change source language.

  // |has_checkmarks| is value initialized full of |false|.
  bool has_checkmarks[std::extent<decltype(titles)>::value] = {};
  int overflow_menu_item_ids[std::extent<decltype(titles)>::value] = {};

  size_t item_count = 0U;

  const std::u16string& source_language_display_name =
      ui_delegate_->GetLanguageNameAt(ui_delegate_->GetSourceLanguageIndex());

  // "More languages".
  CHECK_GT(std::extent<decltype(titles)>::value, item_count);
  titles[item_count] =
      l10n_util::GetStringUTF16(IDS_TRANSLATE_OPTION_MORE_LANGUAGE);
  overflow_menu_item_ids[item_count++] =
      static_cast<int>(OverflowMenuItemId::kChangeTargetLanguage);

  // Menu item divider.
  CHECK_GT(std::extent<decltype(titles)>::value, item_count);
  overflow_menu_item_ids[item_count++] =
      static_cast<int>(OverflowMenuItemId::kInvalid);

  if (!IsIncognito() &&
      ui_delegate_->GetSourceLanguageCode() != kUnknownLanguageCode) {
    // "Always translate pages in <source language>".
    CHECK_GT(std::extent<decltype(titles)>::value, item_count);
    titles[item_count] = l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_MESSAGE_ALWAYS_TRANSLATE_LANGUAGE,
        source_language_display_name);
    has_checkmarks[item_count] = ui_delegate_->ShouldAlwaysTranslate();
    overflow_menu_item_ids[item_count++] =
        static_cast<int>(OverflowMenuItemId::kToggleAlwaysTranslateLanguage);
  }

  if (ui_delegate_->GetSourceLanguageCode() != kUnknownLanguageCode) {
    // "Never translate pages in <source language>".
    CHECK_GT(std::extent<decltype(titles)>::value, item_count);
    titles[item_count] = l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_MESSAGE_NEVER_TRANSLATE_LANGUAGE,
        source_language_display_name);
    has_checkmarks[item_count] = ui_delegate_->IsLanguageBlocked();
    overflow_menu_item_ids[item_count++] =
        static_cast<int>(OverflowMenuItemId::kToggleNeverTranslateLanguage);
  }

  if (ui_delegate_->CanAddSiteToNeverPromptList()) {
    // "Never translate this site".
    CHECK_GT(std::extent<decltype(titles)>::value, item_count);
    titles[item_count] =
        l10n_util::GetStringUTF16(IDS_TRANSLATE_NEVER_TRANSLATE_SITE);
    has_checkmarks[item_count] = ui_delegate_->IsSiteOnNeverPromptList();
    overflow_menu_item_ids[item_count++] =
        static_cast<int>(OverflowMenuItemId::kToggleNeverTranslateSite);
  }

  // "Page is not in <source language>?".
  CHECK_GT(std::extent<decltype(titles)>::value, item_count);
  titles[item_count] = l10n_util::GetStringFUTF16(
      IDS_TRANSLATE_INFOBAR_OPTIONS_NOT_SOURCE_LANGUAGE,
      source_language_display_name);
  overflow_menu_item_ids[item_count++] =
      static_cast<int>(OverflowMenuItemId::kChangeSourceLanguage);

  // Pass arrays of empty strings for both |subtitles| and |language_codes|.
  std::u16string subtitles[std::extent<decltype(titles)>::value];
  std::string language_codes[std::extent<decltype(titles)>::value];

  return bridge_->ConstructMenuItemArray(
      env,
      base::android::ToJavaArrayOfStrings(env,
                                          base::make_span(titles, item_count)),
      base::android::ToJavaArrayOfStrings(
          env, base::make_span(subtitles, item_count)),
      base::android::ToJavaBooleanArray(env, has_checkmarks, item_count),
      base::android::ToJavaIntArray(env, overflow_menu_item_ids, item_count),
      base::android::ToJavaArrayOfStrings(
          env, base::make_span(language_codes, item_count)));
}

base::android::ScopedJavaLocalRef<jobjectArray>
TranslateMessage::HandleSecondaryMenuItemClicked(
    JNIEnv* env,
    jint overflow_menu_item_id,
    const base::android::JavaRef<jstring>& language_code,
    jboolean had_checkmark) {
  std::string language_code_utf8 = ConvertJavaStringToUTF8(env, language_code);
  if (!language_code_utf8.empty()) {
    switch (static_cast<OverflowMenuItemId>(overflow_menu_item_id)) {
      case OverflowMenuItemId::kChangeSourceLanguage:
        ui_delegate_->ReportUIInteraction(UIInteraction::kChangeSourceLanguage);
        ui_delegate_->UpdateSourceLanguage(language_code_utf8);
        ui_delegate_->Translate();
        break;

      case OverflowMenuItemId::kChangeTargetLanguage:
        ui_delegate_->ReportUIInteraction(UIInteraction::kChangeTargetLanguage);
        ui_delegate_->UpdateTargetLanguage(language_code_utf8);
        ui_delegate_->Translate();
        break;

      default:
        NOTREACHED();
        break;
    }
    return nullptr;
  }

  const bool desired_toggle_value = !had_checkmark;

  switch (static_cast<OverflowMenuItemId>(overflow_menu_item_id)) {
    case OverflowMenuItemId::kChangeSourceLanguage: {
      const std::string skip_language_codes[] = {
          ui_delegate_->GetSourceLanguageCode()};
      return ConstructLanguagePickerMenu(
          env, OverflowMenuItemId::kChangeSourceLanguage,
          /*content_language_codes=*/base::span<const std::string>(),
          skip_language_codes);
    }

    case OverflowMenuItemId::kChangeTargetLanguage: {
      const std::string skip_language_codes[] = {
          ui_delegate_->GetTargetLanguageCode(), kUnknownLanguageCode};
      std::vector<std::string> content_language_codes;
      ui_delegate_->GetContentLanguagesCodes(&content_language_codes);
      return ConstructLanguagePickerMenu(
          env, OverflowMenuItemId::kChangeTargetLanguage,
          content_language_codes, skip_language_codes);
    }

    case OverflowMenuItemId::kToggleAlwaysTranslateLanguage:
      ui_delegate_->ReportUIInteraction(
          UIInteraction::kAlwaysTranslateLanguage);
      if (ui_delegate_->ShouldAlwaysTranslate() != desired_toggle_value)
        ui_delegate_->SetAlwaysTranslate(desired_toggle_value);

      if (desired_toggle_value &&
          translate_step_ == TRANSLATE_STEP_BEFORE_TRANSLATE) {
        ui_delegate_->Translate();
      }
      break;

    case OverflowMenuItemId::kToggleNeverTranslateLanguage:
      ui_delegate_->ReportUIInteraction(UIInteraction::kNeverTranslateLanguage);
      if (ui_delegate_->IsLanguageBlocked() != desired_toggle_value)
        ui_delegate_->SetLanguageBlocked(desired_toggle_value);

      if (desired_toggle_value &&
          (translate_step_ == TRANSLATE_STEP_TRANSLATING ||
           translate_step_ == TRANSLATE_STEP_AFTER_TRANSLATE)) {
        RevertTranslationAndUpdateMessage();
      }
      break;

    case OverflowMenuItemId::kToggleNeverTranslateSite:
      ui_delegate_->ReportUIInteraction(UIInteraction::kNeverTranslateSite);
      if (ui_delegate_->IsSiteOnNeverPromptList() != desired_toggle_value)
        ui_delegate_->SetNeverPromptSite(desired_toggle_value);

      if (desired_toggle_value &&
          (translate_step_ == TRANSLATE_STEP_TRANSLATING ||
           translate_step_ == TRANSLATE_STEP_AFTER_TRANSLATE)) {
        RevertTranslationAndUpdateMessage();
      }
      break;

    default:
      NOTREACHED();
      break;
  }

  return nullptr;
}

void TranslateMessage::RevertTranslationAndUpdateMessage() {
  // The TranslateManager doesn't make another call to show the pre-translation
  // UI during RevertTranslation(), so show the correct UI here. This is done
  // before calling RevertTranslation() just in case RevertTranslation() causes
  // this message to be dismissed or destroyed, since that could cause a
  // use-after-free.
  ShowTranslateStep(TRANSLATE_STEP_BEFORE_TRANSLATE,
                    ui_delegate_->GetSourceLanguageCode(),
                    ui_delegate_->GetTargetLanguageCode());

  ui_delegate_->RevertTranslation();
}

bool TranslateMessage::IsIncognito() const {
  if (!translate_manager_)
    return false;
  TranslateClient* client = translate_manager_->translate_client();
  TranslateDriver* driver = client->GetTranslateDriver();
  return driver ? driver->IsIncognito() : false;
}

base::android::ScopedJavaLocalRef<jobjectArray>
TranslateMessage::ConstructLanguagePickerMenu(
    JNIEnv* env,
    OverflowMenuItemId overflow_menu_item_id,
    base::span<const std::string> content_language_codes,
    base::span<const std::string> skip_language_codes) const {
  std::vector<std::u16string> titles;
  std::vector<std::u16string> subtitles;
  std::vector<int> overflow_menu_item_ids;
  std::vector<std::string> language_codes;

  // Add the content languages to the menu.
  for (const std::string& content_language_code : content_language_codes) {
    if (std::find(skip_language_codes.begin(), skip_language_codes.end(),
                  content_language_code) != skip_language_codes.end()) {
      continue;
    }

    titles.emplace_back(l10n_util::GetDisplayNameForLocale(
        content_language_code,
        TranslateDownloadManager::GetInstance()->application_locale(),
        /*is_for_ui=*/true));
    subtitles.emplace_back(l10n_util::GetDisplayNameForLocale(
        content_language_code, content_language_code, /*is_for_ui=*/true));
    overflow_menu_item_ids.emplace_back(
        static_cast<int>(overflow_menu_item_id));
    language_codes.emplace_back(content_language_code);
  }

  if (!titles.empty()) {
    // Add a divider between the content languages and the later full list of
    // languages.
    titles.emplace_back(std::u16string());
    subtitles.emplace_back(std::u16string());
    overflow_menu_item_ids.emplace_back(
        static_cast<int>(OverflowMenuItemId::kInvalid));
    language_codes.emplace_back(std::string());
  }

  // Add the full list of languages to the menu.
  for (size_t i = 0U; i < ui_delegate_->GetNumberOfLanguages(); ++i) {
    std::string code = ui_delegate_->GetLanguageCodeAt(i);
    if (std::find(skip_language_codes.begin(), skip_language_codes.end(),
                  code) != skip_language_codes.end()) {
      continue;
    }

    titles.emplace_back(ui_delegate_->GetLanguageNameAt(i));
    subtitles.emplace_back(std::u16string());
    overflow_menu_item_ids.emplace_back(
        static_cast<int>(overflow_menu_item_id));
    language_codes.emplace_back(std::move(code));
  }

  return bridge_->ConstructMenuItemArray(
      env, base::android::ToJavaArrayOfStrings(env, titles),
      base::android::ToJavaArrayOfStrings(env, subtitles),
      /*has_checkmarks=*/
      base::android::ToJavaBooleanArray(
          env, std::make_unique<bool[]>(titles.size()).get(), titles.size()),
      base::android::ToJavaIntArray(env, overflow_menu_item_ids),
      base::android::ToJavaArrayOfStrings(env, language_codes));
}

}  // namespace translate
