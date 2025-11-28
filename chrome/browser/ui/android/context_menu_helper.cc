// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/context_menu_helper.h"

#include <stdint.h>

#include <map>

#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "components/embedder_support/android/contextmenu/context_menu_builder.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
#include "chrome/browser/extensions/extension_menu_model_android.h"
#include "ui/menus/simple_menu_model.h"
#endif  // BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ContextMenuHelper_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

ContextMenuHelper::ContextMenuHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<ContextMenuHelper>(*web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env, Java_ContextMenuHelper_create(env, reinterpret_cast<int64_t>(this),
                                         web_contents->GetJavaWebContents()));
  DCHECK(!java_obj_.is_null());
}

ContextMenuHelper::~ContextMenuHelper() {
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  extension_menu_model_.reset();
#endif  // BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_destroy(env, java_obj_);
}

void ContextMenuHelper::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  JNIEnv* env = base::android::AttachCurrentThread();
  context_menu_params_ = params;
  gfx::NativeView view = GetWebContents().GetNativeView();

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  // Reset any previous menu model, in case a new menu is shown
  // before the old one was gracefully closed.
  extension_menu_model_.reset();
  extension_menu_model_ = std::make_unique<extensions::ExtensionMenuModel>(
      render_frame_host, params);
  extension_menu_model_->PopulateModel();
  ui::MenuModel* model_ptr = extension_menu_model_.get();
#else
  ui::MenuModel* model_ptr = nullptr;
#endif  // BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)

  Java_ContextMenuHelper_showContextMenu(
      env, java_obj_,
      context_menu::BuildJavaContextMenuParams(
          context_menu_params_, model_ptr,
          render_frame_host.GetProcess()->GetDeprecatedID(),
          render_frame_host.GetFrameToken().value()),
      render_frame_host.GetJavaRenderFrameHost(), view->GetContainerView(),
      view->content_offset() * view->GetDipScale());
}

void ContextMenuHelper::DismissContextMenu() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_dismissContextMenu(env, java_obj_);
}

void ContextMenuHelper::OnContextMenuClosed(JNIEnv* env) {
  GetWebContents().NotifyContextMenuClosed(context_menu_params_.link_followed,
                                           context_menu_params_.impression);
}

void ContextMenuHelper::SetPopulatorFactory(
    const JavaRef<jobject>& jpopulator_factory) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_setPopulatorFactory(env, java_obj_,
                                             jpopulator_factory);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextMenuHelper);

DEFINE_JNI(ContextMenuHelper)
