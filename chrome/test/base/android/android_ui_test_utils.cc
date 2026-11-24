// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/android/android_ui_test_utils.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/run_loop.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/test/test_support_jni_headers/LayoutTestBridge_jni.h"

using content::BrowserContext;
using content::WebContents;

namespace android_ui_test_utils {

void OpenUrlInNewTab(BrowserContext* context,
                     WebContents* parent,
                     const GURL& url) {
  TabModel* tab_model = TabModelList::GetTabModelForWebContents(parent);
  ASSERT_EQ(parent, tab_model->GetActiveWebContents());
  int tab_count = tab_model->GetTabCount();
  ASSERT_GT(tab_count, 0);

  // Create a new tab.
  std::unique_ptr<WebContents> contents =
      WebContents::Create(WebContents::CreateParams(context));
  WebContents* raw_web_contents = contents.get();
  tab_model->CreateTab(TabAndroid::FromWebContents(parent), std::move(contents),
                       TabModel::kInvalidIndex,
                       TabModel::TabLaunchType::FROM_RECENT_TABS_FOREGROUND,
                       /*should_pin=*/false);

  content::NavigateToURLBlockUntilNavigationsComplete(raw_web_contents, url, 1);
  ASSERT_EQ(tab_count + 1, tab_model->GetTabCount());
  ASSERT_NE(parent, raw_web_contents);
  ASSERT_EQ(raw_web_contents, tab_model->GetActiveWebContents());
}

void WaitForBrowsingLayout(BrowserWindowInterface* browser) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::RunLoop run_loop;
  auto wait = LayoutTestBridgeJni::waitForBrowsingLayout(
      env, browser->GetWindow()->GetNativeWindow(), run_loop.QuitClosure());
  run_loop.Run();
  // The run loop can also end without the layout settling, e.g. on a timeout.
  wait->destroy(env);
}

}  // namespace android_ui_test_utils
