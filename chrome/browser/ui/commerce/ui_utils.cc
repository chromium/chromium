// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/ui_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_utils.h"
#include "content/public/browser/navigation_controller.h"

namespace commerce {

void OpenProductSpecsTabForUrls(const std::vector<GURL>& urls,
                                TabStripModel* tab_strip_model,
                                int index) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(tab_strip_model->profile()));

  web_contents->GetController().LoadURL(commerce::GetProductSpecsTabUrl(urls),
                                        content::Referrer(),
                                        ui::PAGE_TRANSITION_LINK, "");

  tab_strip_model->AddWebContents(std::move(web_contents), index,
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                  ADD_ACTIVE);
}

void ShowProductSpecsConfirmationToast(std::u16string set_name,
                                       ToastController* toast_controller) {
  if (!toast_controller) {
    return;
  }

  ToastParams params(ToastId::kAddedToComparisonTable);
  params.body_string_replacement_params = {set_name};
  toast_controller->MaybeShowToast(std::move(params));
}

}  // namespace commerce
