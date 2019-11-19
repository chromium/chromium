// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/context_menu_helper.h"

#include <stdint.h>

#include <map>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "chrome/android/chrome_jni_headers/ContextMenuHelper_jni.h"
#include "chrome/android/chrome_jni_headers/ContextMenuParams_jni.h"
#include "chrome/browser/download/android/download_controller_base.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/context_menu_params.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/context_menu_data/media_type.h"
#include "ui/android/view_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace {

class ContextMenuHelperImageRequest : public ImageDecoder::ImageRequest {
 public:
  static void Start(const base::android::JavaRef<jobject>& jcallback,
                    const std::vector<uint8_t>& thumbnail_data) {
    ContextMenuHelperImageRequest* request =
        new ContextMenuHelperImageRequest(jcallback);
    ImageDecoder::Start(request, thumbnail_data);
  }

 protected:
  void OnImageDecoded(const SkBitmap& decoded_image) override {
    base::android::RunObjectCallbackAndroid(
        jcallback_, gfx::ConvertToJavaBitmap(&decoded_image));
    delete this;
  }

  void OnDecodeImageFailed() override {
    base::android::ScopedJavaLocalRef<jobject> j_bitmap;
    base::android::RunObjectCallbackAndroid(jcallback_, j_bitmap);
    delete this;
  }

 private:
  ContextMenuHelperImageRequest(
      const base::android::JavaRef<jobject>& jcallback)
      : jcallback_(jcallback) {}

  const base::android::ScopedJavaGlobalRef<jobject> jcallback_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContextMenuHelperImageRequest);
};

void OnRetrieveImageForShare(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const base::android::JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size) {
  base::android::RunByteArrayCallbackAndroid(jcallback, thumbnail_data);
}

void OnRetrieveImageForContextMenu(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const base::android::JavaRef<jobject>& jcallback,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size) {
  ContextMenuHelperImageRequest::Start(jcallback, thumbnail_data);
}

}  // namespace

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
  render_frame_id_ = render_frame_host->GetRoutingID();
  render_process_id_ = render_frame_host->GetProcess()->GetID();
  gfx::NativeView view = web_contents_->GetNativeView();
  Java_ContextMenuHelper_showContextMenu(
      env, java_obj_, ContextMenuHelper::CreateJavaContextMenuParams(params),
      view->GetContainerView(), view->content_offset() * view->GetDipScale());
}

void ContextMenuHelper::OnContextMenuClosed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  web_contents_->NotifyContextMenuClosed(context_menu_params_.custom_context);
}

void ContextMenuHelper::SetPopulator(const JavaRef<jobject>& jpopulator) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_setPopulator(env, java_obj_, jpopulator);
}

base::android::ScopedJavaLocalRef<jobject>
ContextMenuHelper::CreateJavaContextMenuParams(
    const content::ContextMenuParams& params) {
  GURL sanitizedReferrer = (params.frame_url.is_empty() ?
      params.page_url : params.frame_url).GetAsReferrer();

  bool can_save = params.media_flags & blink::WebContextMenuData::kMediaCanSave;
  JNIEnv* env = base::android::AttachCurrentThread();
  base::string16 title_text =
      (params.title_text.empty() ? params.alt_text : params.title_text);

  base::android::ScopedJavaLocalRef<jobject> jmenu_info =
      ContextMenuParamsAndroid::Java_ContextMenuParams_create(
          env, static_cast<int>(params.media_type),
          ConvertUTF8ToJavaString(env, params.page_url.spec()),
          ConvertUTF8ToJavaString(env, params.link_url.spec()),
          ConvertUTF16ToJavaString(env, params.link_text),
          ConvertUTF8ToJavaString(env, params.unfiltered_link_url.spec()),
          ConvertUTF8ToJavaString(env, params.src_url.spec()),
          ConvertUTF16ToJavaString(env, title_text),
          ConvertUTF8ToJavaString(env, sanitizedReferrer.spec()),
          static_cast<int>(params.referrer_policy), can_save, params.x,
          params.y, params.source_type);

  return jmenu_info;
}

base::android::ScopedJavaLocalRef<jobject>
ContextMenuHelper::GetJavaWebContents(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  return web_contents_->GetJavaWebContents();
}

void ContextMenuHelper::OnStartDownload(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean jis_link,
    jboolean jis_data_reduction_proxy_enabled) {
  std::string headers;
  if (jis_data_reduction_proxy_enabled)
    headers = data_reduction_proxy::chrome_proxy_pass_through_header();

  DownloadControllerBase::Get()->StartContextMenuDownload(
      context_menu_params_,
      web_contents_,
      jis_link,
      headers);
}

void ContextMenuHelper::SearchForImage(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;

  CoreTabHelper::FromWebContents(web_contents_)->SearchByImageInNewTab(
      render_frame_host, context_menu_params_.src_url);
}

void ContextMenuHelper::RetrieveImageForShare(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px) {
  RetrieveImageInternal(env, base::Bind(&OnRetrieveImageForShare), jcallback,
                        max_width_px, max_height_px);
}

void ContextMenuHelper::RetrieveImageForContextMenu(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px) {
  RetrieveImageInternal(env, base::Bind(&OnRetrieveImageForContextMenu),
                        jcallback, max_width_px, max_height_px);
}

void ContextMenuHelper::RetrieveImageInternal(
    JNIEnv* env,
    const ImageRetrieveCallback& retrieve_callback,
    const JavaParamRef<jobject>& jcallback,
    jint max_width_px,
    jint max_height_px) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);
  // Bind the InterfacePtr into the callback so that it's kept alive
  // until there's either a connection error or a response.
  auto* thumbnail_capturer_proxy = chrome_render_frame.get();
  thumbnail_capturer_proxy->RequestThumbnailForContextNode(
      0, gfx::Size(max_width_px, max_height_px),
      chrome::mojom::ImageFormat::PNG,
      base::Bind(retrieve_callback, base::Passed(&chrome_render_frame),
                 base::android::ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextMenuHelper)
