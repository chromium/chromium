// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_CONTEXT_MENU_HELPER_H_
#define CHROME_BROWSER_UI_ANDROID_CONTEXT_MENU_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/optimization_guide/proto/performance_hints_metadata.pb.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class ContextMenuHelper
    : public content::WebContentsUserData<ContextMenuHelper> {
 public:
  ~ContextMenuHelper() override;

  void ShowContextMenu(content::RenderFrameHost* render_frame_host,
                       const content::ContextMenuParams& params);

  void OnContextMenuClosed(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);

  void SetPopulatorFactory(
      const base::android::JavaRef<jobject>& jpopulator_factory);

 private:
  explicit ContextMenuHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ContextMenuHelper>;

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
  GetChromeRenderFrame() const;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  content::WebContents* web_contents_;

  content::ContextMenuParams context_menu_params_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ContextMenuHelper);
};

#endif  // CHROME_BROWSER_UI_ANDROID_CONTEXT_MENU_HELPER_H_
