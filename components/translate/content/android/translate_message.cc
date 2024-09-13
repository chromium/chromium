// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/translate/content/android/translate_message.h"

#include <stddef.h>
#include <stdint.h>

#include <type_traits>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "base/containers/heap_array.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/messages/android/message_enums.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/browser/translate_ui_languages_manager.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_metrics.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/translate/content/android/jni_headers/TranslateMessage_jni.h"

namespace translate {

namespace {

constexpr int kDismissalDurationSeconds = 10;

// Default implementation of the TranslateMessage::Bridge interface, which just
// calls the appropriate Java methods in each case.
class BridgeImpl : public TranslateMessage::Bridge {
 public:
  ~BridgeImpl() override;

  bool CreateTranslateMessage(JNIEnv* env,
                              content::WebContents* web_contents,
                              TranslateMessage* native_translate_message,
                              jint dismissal_duration_seconds) override {
    DCHECK(!java_translate_message_);
    java_translate_message_ = Java_TranslateMessage_create(
        env, web_contents->GetJavaWebContents(),
        reinterpret_cast<intptr_t>(native_translate_message),
        dismissal_duration_seconds);
    return !(!java_translate_message_);
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
      base::android::ScopedJavaLocalRef<jstring> primary_button_text,
      jboolean has_overflow_menu) override {
    Java_TranslateMessage_showMessage(
        env, java_translate_message_, std::move(title), std::move(description),
        std::move(primary_button_text), has_overflow_menu);
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
    if (java_translate_message_)
      Java_TranslateMessage_clearNativePointer(env, java_translate_message_);
    java_translate_message_ = nullptr;
  }

  void Dismiss(JNIEnv* env) override {
    if (java_translate_message_)
      Java_TranslateMessage_dismiss(env, java_translate_message_);
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_translate_message_;
};

BridgeImpl::~BridgeImpl() = default;

base::android::ScopedJavaLocalRef<jstring> GetDefaultMessageDescription(
    JNIEnv* env,
    const std::u16string& source_language_display_name,
    const std::u16string& target_language_display_name) {
  return base::android::ConvertUTF16ToJavaString(
      env, l10n_util::GetStringFUTF16(IDS_TRANSLATE_MESSAGE_DESCRIPTION,
                                      source_language_display_name,
                                      target_language_display_name));
}

}  // namespace

TranslateMessage::Bridge::~Bridge() = default;

TranslateMessage::TranslateMessage(
    content::WebContents* web_contents,
    const base::WeakPtr<TranslateManager>& translate_manager,
    base::RepeatingCallback<void()> on_dismiss_callback,
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
    base::RepeatingCallback<void()> on_dismiss_callback)
    : TranslateMessage(web_contents,
                       translate_manager,
                       std::move(on_dismiss_callback),
                       std::make_unique<BridgeImpl>()) {}

TranslateMessage::~TranslateMessage() {
  // Clear the |on_dismiss_callback_| so that it doesn't get run during object
  // destruction. This prevents a possible use-after-free if the callback points
  // to a method on the owner of |this|.
  on_dismiss_callback_.Reset();

  JNIEnv* env = base::android::AttachCurrentThread();
  if (state_ != State::kDismissed)
    bridge_->Dismiss(env);
}

void TranslateMessage::ShowTranslateStep(TranslateStep step,
                                         const std::string& source_language,
                                         const std::string& target_language) {
  DCHECK(!on_dismiss_callback_.is_null());
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!ui_delegate_) {
    ui_delegate_ = std::make_unique<TranslateUIDelegate>(
        translate_manager_, source_language, target_language);
    ui_languages_manager_ = ui_delegate_->translate_ui_languages_manager();
  }

  if (state_ == State::kDismissed) {
    if (!bridge_->CreateTranslateMessage(env, web_contents_, this,
                                         kDismissalDurationSeconds)) {
      // The |bridge_| failed to create the Java TranslateMessage, such as when
      // the activity is being destroyed, so there is no message to show.
      return;
    }

    ReportCompactInfobarEvent(InfobarEvent::INFOBAR_IMPRESSION);
  }

  ui_delegate_->UpdateAndRecordSourceLanguage(source_language);
  ui_delegate_->UpdateAndRecordTargetLanguage(target_language);

  if (step == TRANSLATE_STEP_TRANSLATE_ERROR) {
    // Prevent auto-always-translate from triggering if an error occurs.
    is_translation_eligible_for_auto_always_translate_ = false;

    // Count an error as an interaction so that the translation ignored/denied
    // counts don't rise in response to errors. For example, suppose a user
    // wants to translate a webpage that's in Hindi, and has "Always translate
    // pages in Hindi" turned on, but for some reason repeatedly gets
    // translation errors and repeatedly swipes away the message and refreshes
    // the page trying to make translation work. If these are counted as
    // translations being denied, then this could affect decisions made by the
    // translate ranker or cause auto-never-translate-language to trigger
    // inadvertently.
    has_been_interacted_with_ = true;

    bridge_->ShowTranslateError(env, web_contents_);

    // Since an error occurred, show the UI in the last good state.
    const LanguageState* language_state = ui_delegate_->GetLanguageState();
    if (language_state && language_state->IsPageTranslated())
      step = TRANSLATE_STEP_AFTER_TRANSLATE;
    else
      step = TRANSLATE_STEP_BEFORE_TRANSLATE;
  }

  const std::u16string& source_language_display_name =
      ui_languages_manager_->GetLanguageNameAt(
          ui_languages_manager_->GetSourceLanguageIndex());
  const std::u16string& target_language_display_name =
      ui_languages_manager_->GetLanguageNameAt(
          ui_languages_manager_->GetTargetLanguageIndex());

  base::android::ScopedJavaLocalRef<jstring> title;
  base::android::ScopedJavaLocalRef<jstring> description;
  base::android::ScopedJavaLocalRef<jstring> primary_button_text;

  switch (step) {
    case TRANSLATE_STEP_BEFORE_TRANSLATE:
      title = base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(
                   IDS_TRANSLATE_MESSAGE_BEFORE_TRANSLATE_TITLE));
      description = GetDefaultMessageDescription(
          env, source_language_display_name, target_language_display_name);
      primary_button_text = base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(IDS_TRANSLATE_BUTTON));

      state_ = State::kBeforeTranslate;
      is_translation_eligible_for_auto_always_translate_ = false;
      break;

    case TRANSLATE_STEP_TRANSLATING:
      if (state_ == State::kDismissed) {
        // If the UI is currently not shown and being opened directly into the
        // translation-in-progress state (e.g. if the page was loaded and
        // "Always translate pages in <language>" triggered, or if the
        // "Translate" menu item was clicked in the browser 3-dots menu), show a
        // separate title string indicating that the translation is in progress
        // instead of the default "Translate page?" title used in the
        // before-translate state.
        title = base::android::ConvertUTF16ToJavaString(
            env, l10n_util::GetStringUTF16(
                     IDS_TRANSLATE_MESSAGE_TRANSLATING_COLD_OPEN_TITLE));
      } else {
        title = base::android::ConvertUTF16ToJavaString(
            env, l10n_util::GetStringUTF16(
                     IDS_TRANSLATE_MESSAGE_BEFORE_TRANSLATE_TITLE));
      }
      description = GetDefaultMessageDescription(
          env, source_language_display_name, target_language_display_name);
      primary_button_text = nullptr;

      state_ = State::kTranslating;
      break;

    case TRANSLATE_STEP_AFTER_TRANSLATE:
      title = base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(
                   IDS_TRANSLATE_MESSAGE_AFTER_TRANSLATE_TITLE));
      primary_button_text = base::android::ConvertUTF16ToJavaString(
          env, l10n_util::GetStringUTF16(IDS_TRANSLATE_MESSAGE_UNDO_BUTTON));

      if (is_translation_eligible_for_auto_always_translate_ &&
          ui_delegate_->ShouldAutoAlwaysTranslate()) {
        ReportCompactInfobarEvent(
            InfobarEvent::INFOBAR_SNACKBAR_AUTO_ALWAYS_IMPRESSION);
        ui_delegate_->SetAlwaysTranslate(true);

        description = base::android::ConvertUTF16ToJavaString(
            env,
            l10n_util::GetStringFUTF16(
                IDS_TRANSLATE_MESSAGE_AUTO_ALWAYS_TRANSLATE_LANGUAGE_DESCRIPTION,
                source_language_display_name, target_language_display_name));

        state_ = State::kAfterTranslateWithAutoAlwaysConfirmation;
      } else {
        description = GetDefaultMessageDescription(
            env, source_language_display_name, target_language_display_name);
        state_ = State::kAfterTranslate;
      }

      is_translation_eligible_for_auto_always_translate_ = false;
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  bridge_->ShowMessage(env, std::move(title), std::move(description),
                       std::move(primary_button_text),
                       /*has_overflow_menu=*/true);
}

void TranslateMessage::HandlePrimaryAction(JNIEnv* env) {
  has_been_interacted_with_ = true;
  is_translation_eligible_for_auto_always_translate_ = false;

  switch (state_) {
    case State::kBeforeTranslate:
      ReportCompactInfobarEvent(InfobarEvent::INFOBAR_TARGET_TAB_TRANSLATE);
      is_translation_eligible_for_auto_always_translate_ = true;
      ui_delegate_->ReportUIInteraction(UIInteraction::kTranslate);
      ui_delegate_->Translate();
      break;

    case State::kTranslating:
      // Should not happen, but per https://crbug.com/1409304 it may, so add
      // logging.
      base::debug::DumpWithoutCrashing();
      break;
    case State::kAfterTranslateWithAutoAlwaysConfirmation:
      // The user clicked "Undo" on a translated page when the
      // auto-always-translate confirmation message was showing, so turn off
      // "always translate language" before reverting the translation.
      ReportCompactInfobarEvent(
          InfobarEvent::INFOBAR_SNACKBAR_CANCEL_AUTO_ALWAYS);
      ui_delegate_->SetAlwaysTranslate(false);
      [[fallthrough]];
    case State::kAfterTranslate:
      ReportCompactInfobarEvent(InfobarEvent::INFOBAR_REVERT);
      ui_delegate_->ReportUIInteraction(UIInteraction::kRevert);
      RevertTranslationAndUpdateMessage();
      break;

    case State::kAutoNeverTranslateConfirmation:
      // The user clicked "Undo" on the message confirming that pages in this
      // language will not be translated, so unblock that language. Also, since
      // this confirmation message is only shown after the user has already
      // tried to dismiss the translate UI, dismiss this popup as well.
      ReportCompactInfobarEvent(
          InfobarEvent::INFOBAR_SNACKBAR_CANCEL_AUTO_NEVER);
      ui_delegate_->SetLanguageBlocked(false);
      bridge_->Dismiss(env);
      break;
    case State::kDismissed:
      // Should not happen, but per https://crbug.com/1409304 it may, so add
      // logging.
      base::debug::DumpWithoutCrashing();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void TranslateMessage::HandleDismiss(JNIEnv* env, jint dismiss_reason) {
  switch (static_cast<messages::DismissReason>(dismiss_reason)) {
    case messages::DismissReason::GESTURE:
      ui_delegate_->OnUIClosedByUser();
      ui_delegate_->ReportUIInteraction(UIInteraction::kCloseUIExplicitly);
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

    case messages::DismissReason::PRIMARY_ACTION:
    case messages::DismissReason::SECONDARY_ACTION:
      // These dismiss reasons should not be possible for a TranslateMessage,
      // since clicking the primary or secondary buttons doesn't dismiss the
      // message.
      NOTREACHED_IN_MIGRATION();
      break;

    default:
      break;
  }

  if (!has_been_interacted_with_ && state_ == State::kBeforeTranslate) {
    ReportCompactInfobarEvent(InfobarEvent::INFOBAR_DECLINE);

    // In order to have the same off-by-one counting as the infobar UI,
    // ShouldAutoNeverTranslate() must be called before TranslationDeclined().
    const bool should_auto_never_translate =
        static_cast<messages::DismissReason>(dismiss_reason) ==
            messages::DismissReason::GESTURE &&
        ui_delegate_->ShouldAutoNeverTranslate();

    ui_delegate_->TranslationDeclined(
        static_cast<messages::DismissReason>(dismiss_reason) ==
        messages::DismissReason::GESTURE);

    if (should_auto_never_translate) {
      ReportCompactInfobarEvent(
          InfobarEvent::INFOBAR_SNACKBAR_AUTO_NEVER_IMPRESSION);

      ui_delegate_->SetLanguageBlocked(true);
      state_ = State::kAutoNeverTranslateConfirmation;

      bridge_->ShowMessage(
          env, /*title=*/
          base::android::ConvertUTF16ToJavaString(
              env,
              l10n_util::GetStringFUTF16(
                  IDS_TRANSLATE_MESSAGE_AUTO_NEVER_TRANSLATE_LANGUAGE_TITLE,
                  ui_languages_manager_->GetLanguageNameAt(
                      ui_languages_manager_->GetSourceLanguageIndex()))),
          /*description=*/nullptr,
          /*primary_button_text=*/
          base::android::ConvertUTF16ToJavaString(
              env, l10n_util::GetStringUTF16(IDS_TRANSLATE_NOTIFICATION_UNDO)),
          /*has_overflow_menu=*/false);

      // Return early here without calling the dismiss callback, since the
      // dismiss callback could try to do things like show an IPH tooltip, which
      // would be obscured by the auto-never-translate confirmation message.
      return;
    }
  }

  bridge_->ClearNativePointer(env);
  state_ = State::kDismissed;

  // The only time |on_dismiss_callback_| will be null is during the destruction
  // of |this|.
  if (!on_dismiss_callback_.is_null()) {
    // Note that this callback can destroy |this|, so this method shouldn't do
    // anything afterwards.
    on_dismiss_callback_.Run();
  }
}

base::android::ScopedJavaLocalRef<jobjectArray>
TranslateMessage::BuildOverflowMenu(JNIEnv* env) {
  ReportCompactInfobarEvent(InfobarEvent::INFOBAR_OPTIONS);

  has_been_interacted_with_ = true;

  // If the overflow menu is open when auto-always-translate triggers, then the
  // "Always translate language" option in the menu would remain unchecked even
  // if auto-always-translate triggers and changes that setting. To avoid this
  // inconsistency, don't try to turn on auto-always-translate if the user
  // opened the overflow menu mid-translation.
  is_translation_eligible_for_auto_always_translate_ = false;

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
      ui_languages_manager_->GetLanguageNameAt(
          ui_languages_manager_->GetSourceLanguageIndex());

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

  if (!ui_delegate_->IsIncognito() &&
      ui_languages_manager_->GetSourceLanguageCode() != kUnknownLanguageCode) {
    // "Always translate pages in <source language>".
    CHECK_GT(std::extent<decltype(titles)>::value, item_count);
    titles[item_count] = l10n_util::GetStringFUTF16(
        IDS_TRANSLATE_MESSAGE_ALWAYS_TRANSLATE_LANGUAGE,
        source_language_display_name);
    has_checkmarks[item_count] = ui_delegate_->ShouldAlwaysTranslate();
    overflow_menu_item_ids[item_count++] =
        static_cast<int>(OverflowMenuItemId::kToggleAlwaysTranslateLanguage);
  }

  if (ui_languages_manager_->GetSourceLanguageCode() != kUnknownLanguageCode) {
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
                                          base::span(titles).first(item_count)),
      base::android::ToJavaArrayOfStrings(
          env, base::span(subtitles).first(item_count)),
      base::android::ToJavaBooleanArray(
          env, base::span(has_checkmarks).first(item_count)),
      base::android::ToJavaIntArray(
          env, base::span(overflow_menu_item_ids).first(item_count)),
      base::android::ToJavaArrayOfStrings(
          env, base::span(language_codes).first(item_count)));
}

base::android::ScopedJavaLocalRef<jobjectArray>
TranslateMessage::HandleSecondaryMenuItemClicked(
    JNIEnv* env,
    jint overflow_menu_item_id,
    const base::android::JavaRef<jstring>& language_code,
    jboolean had_checkmark) {
  has_been_interacted_with_ = true;

  // Interacting with the secondary menu can cause the page to be translated or
  // an existing translation to be reverted, so to avoid these edge cases, don't
  // try to turn on auto-always-translate.
  is_translation_eligible_for_auto_always_translate_ = false;

  std::string language_code_utf8 =
      base::android::ConvertJavaStringToUTF8(env, language_code);
  if (!language_code_utf8.empty()) {
    switch (static_cast<OverflowMenuItemId>(overflow_menu_item_id)) {
      case OverflowMenuItemId::kChangeSourceLanguage:
        ui_delegate_->ReportUIInteraction(UIInteraction::kChangeSourceLanguage);
        ui_delegate_->UpdateAndRecordSourceLanguage(language_code_utf8);
        ui_delegate_->Translate();
        break;

      case OverflowMenuItemId::kChangeTargetLanguage:
        ReportCompactInfobarEvent(
            InfobarEvent::INFOBAR_MORE_LANGUAGES_TRANSLATE);
        ui_delegate_->ReportUIInteraction(UIInteraction::kChangeTargetLanguage);
        ui_delegate_->UpdateAndRecordTargetLanguage(language_code_utf8);
        ui_delegate_->Translate();
        break;

      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
    return nullptr;
  }

  const bool desired_toggle_value = !had_checkmark;

  switch (static_cast<OverflowMenuItemId>(overflow_menu_item_id)) {
    case OverflowMenuItemId::kChangeSourceLanguage: {
      ReportCompactInfobarEvent(InfobarEvent::INFOBAR_PAGE_NOT_IN);
      const std::string skip_language_codes[] = {
          ui_languages_manager_->GetSourceLanguageCode()};
      return ConstructLanguagePickerMenu(
          env, OverflowMenuItemId::kChangeSourceLanguage,
          /*content_language_codes=*/base::span<const std::string>(),
          skip_language_codes);
    }

    case OverflowMenuItemId::kChangeTargetLanguage: {
      ReportCompactInfobarEvent(InfobarEvent::INFOBAR_MORE_LANGUAGES);
      const std::string skip_language_codes[] = {
          ui_languages_manager_->GetTargetLanguageCode(), kUnknownLanguageCode};
      std::vector<std::string> content_language_codes;
      ui_delegate_->GetContentLanguagesCodes(&content_language_codes);
      return ConstructLanguagePickerMenu(
          env, OverflowMenuItemId::kChangeTargetLanguage,
          content_language_codes, skip_language_codes);
    }

    case OverflowMenuItemId::kToggleAlwaysTranslateLanguage:
      if (ui_delegate_->ShouldAlwaysTranslate() != desired_toggle_value) {
        ReportCompactInfobarEvent(
            desired_toggle_value ? InfobarEvent::INFOBAR_ALWAYS_TRANSLATE
                                 : InfobarEvent::INFOBAR_ALWAYS_TRANSLATE_UNDO);
        ui_delegate_->SetAlwaysTranslate(desired_toggle_value);
      }

      if (desired_toggle_value && state_ == State::kBeforeTranslate)
        ui_delegate_->Translate();
      break;

    case OverflowMenuItemId::kToggleNeverTranslateLanguage:
      if (ui_delegate_->IsLanguageBlocked() != desired_toggle_value) {
        ReportCompactInfobarEvent(
            desired_toggle_value ? InfobarEvent::INFOBAR_NEVER_TRANSLATE
                                 : InfobarEvent::INFOBAR_NEVER_TRANSLATE_UNDO);
        ui_delegate_->SetLanguageBlocked(desired_toggle_value);
      }

      if (desired_toggle_value &&
          (state_ == State::kTranslating || state_ == State::kAfterTranslate ||
           state_ == State::kAfterTranslateWithAutoAlwaysConfirmation)) {
        RevertTranslationAndUpdateMessage();
      }
      break;

    case OverflowMenuItemId::kToggleNeverTranslateSite:
      if (ui_delegate_->IsSiteOnNeverPromptList() != desired_toggle_value) {
        ReportCompactInfobarEvent(
            desired_toggle_value
                ? InfobarEvent::INFOBAR_NEVER_TRANSLATE_SITE
                : InfobarEvent::INFOBAR_NEVER_TRANSLATE_SITE_UNDO);
        ui_delegate_->SetNeverPromptSite(desired_toggle_value);
      }

      if (desired_toggle_value &&
          (state_ == State::kTranslating || state_ == State::kAfterTranslate ||
           state_ == State::kAfterTranslateWithAutoAlwaysConfirmation)) {
        RevertTranslationAndUpdateMessage();
      }
      break;

    default:
      NOTREACHED_IN_MIGRATION();
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
                    ui_languages_manager_->GetSourceLanguageCode(),
                    ui_languages_manager_->GetTargetLanguageCode());

  ui_delegate_->RevertTranslation();
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
    if (base::Contains(skip_language_codes, content_language_code)) {
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
  for (size_t i = 0U; i < ui_languages_manager_->GetNumberOfLanguages(); ++i) {
    std::string code = ui_languages_manager_->GetLanguageCodeAt(i);
    if (base::Contains(skip_language_codes, code)) {
      continue;
    }

    titles.emplace_back(ui_languages_manager_->GetLanguageNameAt(i));
    subtitles.emplace_back();
    overflow_menu_item_ids.emplace_back(
        static_cast<int>(overflow_menu_item_id));
    language_codes.emplace_back(std::move(code));
  }

  return bridge_->ConstructMenuItemArray(
      env, base::android::ToJavaArrayOfStrings(env, titles),
      base::android::ToJavaArrayOfStrings(env, subtitles),
      /*has_checkmarks=*/
      base::android::ToJavaBooleanArray(
          env, base::HeapArray<bool>::WithSize(titles.size())),
      base::android::ToJavaIntArray(env, overflow_menu_item_ids),
      base::android::ToJavaArrayOfStrings(env, language_codes));
}

}  // namespace translate
