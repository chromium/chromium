// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/paint_preview/player/android/jni_headers/PlayerAccessibilitySnapshotHelper_jni.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/ax_tree_update.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

ScopedJavaLocalRef<jobject> CreateJavaAXSnapshot(JNIEnv* env,
                                                 const ui::AssistantTree* tree,
                                                 const ui::AssistantNode* node,
                                                 bool is_root) {
  ScopedJavaLocalRef<jstring> j_text =
      base::android::ConvertUTF16ToJavaString(env, node->text);
  ScopedJavaLocalRef<jstring> j_class =
      base::android::ConvertUTF8ToJavaString(env, node->class_name);
  ScopedJavaLocalRef<jobject> j_node =
      Java_PlayerAccessibilitySnapshotHelper_createAccessibilitySnapshotNode(
          env, node->rect.x(), node->rect.y(), node->rect.width(),
          node->rect.height(), is_root, j_text, node->color, node->bgcolor,
          node->text_size, node->bold, node->italic, node->underline,
          node->line_through, j_class);

  if (node->selection.has_value()) {
    Java_PlayerAccessibilitySnapshotHelper_setAccessibilitySnapshotSelection(
        env, j_node, node->selection->start(), node->selection->end());
  }

  for (int child : node->children_indices) {
    Java_PlayerAccessibilitySnapshotHelper_addAccessibilityNodeAsChild(
        env, j_node,
        CreateJavaAXSnapshot(env, tree, tree->nodes[child].get(), false));
  }
  return j_node;
}

}  // namespace

base::android::ScopedJavaLocalRef<jobject>
JNI_PlayerAccessibilitySnapshotHelper_GetAccessibilitySnapshot(
    JNIEnv* env,
    jlong nativeAxTree) {
  std::unique_ptr<ui::AXTreeUpdate> ax_tree_update(
      reinterpret_cast<ui::AXTreeUpdate*>(nativeAxTree));
  std::unique_ptr<ui::AssistantTree> assistant_tree =
      ui::CreateAssistantTree(*ax_tree_update);
  return CreateJavaAXSnapshot(env, assistant_tree.get(),
                              assistant_tree->nodes.front().get(), true);
}
