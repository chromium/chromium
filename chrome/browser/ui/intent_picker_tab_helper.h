// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENT_PICKER_TAB_HELPER_H_
#define CHROME_BROWSER_UI_INTENT_PICKER_TAB_HELPER_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Controls the visibility of IntentPickerView by updating the visibility based
// on stored state.
class IntentPickerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<IntentPickerTabHelper> {
 public:
  ~IntentPickerTabHelper() override;

  static void SetShouldShowIcon(content::WebContents* web_contents,
                                bool should_show_icon);

  bool should_show_icon() const { return should_show_icon_; }

  using IntentPickerIconLoaderCallback =
      base::OnceCallback<void(std::vector<apps::IntentPickerAppInfo> apps)>;

  // Load multiple app icons from App Service.
  static void LoadAppIcons(content::WebContents* web_contents,
                           std::vector<apps::IntentPickerAppInfo> apps,
                           IntentPickerIconLoaderCallback callback);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  explicit IntentPickerTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<IntentPickerTabHelper>;

  void OnAppIconLoaded(std::vector<apps::IntentPickerAppInfo> apps,
                       IntentPickerIconLoaderCallback callback,
                       size_t index,
                       apps::mojom::IconValuePtr icon_value);

  void LoadAppIcon(std::vector<apps::IntentPickerAppInfo> apps,
                   IntentPickerIconLoaderCallback callback,
                   size_t index);

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  bool should_show_icon_ = false;

  base::WeakPtrFactory<IntentPickerTabHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IntentPickerTabHelper);
};

#endif  // CHROME_BROWSER_UI_INTENT_PICKER_TAB_HELPER_H_
