// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/android/note_template_conversion_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/content_creation/notes/android/jni_headers/NoteTemplateConversionBridge_jni.h"
#include "components/content_creation/notes/core/templates/template_types.h"

namespace content_creation {

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {

ScopedJavaLocalRef<jobject> CreateJavaBackground(JNIEnv* env,
                                                 const Background& background) {
  if (background.is_linear_gradient()) {
    const std::vector<int> int_colors(background.colors().begin(),
                                      background.colors().end());
    ScopedJavaLocalRef<jintArray> int_array =
        base::android::ToJavaIntArray(env, int_colors);

    return Java_NoteTemplateConversionBridge_createLinearGradientBackground(
        env, int_array, static_cast<uint16_t>(background.direction()));
  } else if (background.is_image()) {
    return Java_NoteTemplateConversionBridge_createImageBackground(
        env, ConvertUTF8ToJavaString(env, background.image_url()));
  }
  return Java_NoteTemplateConversionBridge_createBackground(env,
                                                            background.color());
}

ScopedJavaLocalRef<jobject> CreateJavaTextStyle(JNIEnv* env,
                                                const TextStyle& text_style) {
  return Java_NoteTemplateConversionBridge_createTextStyle(
      env, ConvertUTF8ToJavaString(env, text_style.font_name()),
      text_style.font_color(), text_style.weight(), text_style.all_caps(),
      static_cast<uint16_t>(text_style.alignment()),
      static_cast<uint16_t>(text_style.min_text_size_sp()),
      static_cast<uint16_t>(text_style.max_text_size_sp()),
      text_style.highlight_color(),
      static_cast<uint16_t>(text_style.highlight_style()));
}

ScopedJavaLocalRef<jobject> CreateJavaFooterStyle(
    JNIEnv* env,
    const FooterStyle& footer_style) {
  return Java_NoteTemplateConversionBridge_createFooterStyle(
      env, footer_style.text_color(), footer_style.logo_color());
}

ScopedJavaLocalRef<jobject> CreateJavaTemplateAndMaybeAddToList(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> jlist,
    const NoteTemplate& note_template) {
  auto jmain_background =
      CreateJavaBackground(env, note_template.main_background());

  ScopedJavaLocalRef<jobject> jcontent_background = nullptr;
  const Background* content_background = note_template.content_background();
  if (content_background) {
    jcontent_background = CreateJavaBackground(env, *content_background);
  }

  auto jtext_style = CreateJavaTextStyle(env, note_template.text_style());
  auto jfooter_style = CreateJavaFooterStyle(env, note_template.footer_style());

  return Java_NoteTemplateConversionBridge_createTemplateAndMaybeAddToList(
      env, jlist, static_cast<uint32_t>(note_template.id()),
      ConvertUTF8ToJavaString(env, note_template.localized_name()),
      jmain_background, jcontent_background, jtext_style, jfooter_style);
}

}  // namespace

// static
ScopedJavaLocalRef<jobject>
NoteTemplateConversionBridge::CreateJavaNoteTemplates(
    JNIEnv* env,
    const std::vector<NoteTemplate>& note_templates) {
  ScopedJavaLocalRef<jobject> jlist =
      Java_NoteTemplateConversionBridge_createTemplateList(env);

  for (const auto& note_template : note_templates) {
    CreateJavaTemplateAndMaybeAddToList(env, jlist, note_template);
  }

  return jlist;
}

}  // namespace content_creation
