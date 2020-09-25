// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/context_menu_helper.h"

#include <stdint.h>

#include <map>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "chrome/android/chrome_jni_headers/ContextMenuHelper_jni.h"
#include "chrome/browser/performance_hints/performance_hints_observer.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/embedder_support/android/contextmenu/context_menu_builder.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/common/context_menu_data/media_type.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using optimization_guide::proto::PerformanceClass;

ContextMenuHelper::ContextMenuHelper(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env, Java_ContextMenuHelper_create(env, reinterpret_cast<int64_t>(this),
                                         web_contents_->GetJavaWebContents())
               .obj());
  DCHECK(!java_obj_.is_null());
}

ContextMenuHelper::~ContextMenuHelper() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_destroy(env, java_obj_);
}

void ContextMenuHelper::ShowContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // TODO(crbug.com/851495): support context menu in VR.
  if (vr::VrTabHelper::IsUiSuppressedInVr(
          web_contents_, vr::UiSuppressedElement::kContextMenu)) {
    web_contents_->NotifyContextMenuClosed(params.custom_context);
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  context_menu_params_ = params;
  gfx::NativeView view = web_contents_->GetNativeView();
  if (!params.link_url.is_empty()) {
    performance_hints::PerformanceHintsObserver::RecordPerformanceUMAForURL(
        web_contents_, params.link_url);
  }
  Java_ContextMenuHelper_showContextMenu(
      env, java_obj_,
      context_menu::BuildJavaContextMenuParams(context_menu_params_),
      render_frame_host->GetJavaRenderFrameHost(), view->GetContainerView(),
      view->content_offset() * view->GetDipScale());
}

void ContextMenuHelper::OnContextMenuClosed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  web_contents_->NotifyContextMenuClosed(context_menu_params_.custom_context);
}

void ContextMenuHelper::SetPopulatorFactory(
    const JavaRef<jobject>& jpopulator_factory) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_setPopulatorFactory(env, java_obj_,
                                             jpopulator_factory);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextMenuHelper)
