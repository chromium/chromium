// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/javascript_dialogs/chrome_javascript_app_modal_dialog_view_factory.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "components/javascript_dialogs/android/app_modal_dialog_view_android.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"

void InstallChromeJavaScriptAppModalDialogViewFactory() {
  javascript_dialogs::AppModalDialogManager::GetInstance()
      ->SetNativeDialogFactory(base::BindRepeating(
          [](javascript_dialogs::AppModalDialogController* controller)
              -> javascript_dialogs::AppModalDialogView* {
            return new javascript_dialogs::AppModalDialogViewAndroid(
                base::android::AttachCurrentThread(), controller,
                controller->web_contents()->GetTopLevelNativeWindow());
          }));
}
