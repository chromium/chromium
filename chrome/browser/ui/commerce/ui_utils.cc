// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commerce/ui_utils.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/commerce/core/commerce_feature_list.h"

namespace commerce {

void ShowProductSpecsConfirmationToast(std::u16string set_name,
                                       Browser* browser) {
  if (!base::FeatureList::IsEnabled(commerce::kCompareConfirmationToast)) {
    return;
  }

  ToastController* const toast_controller =
      browser->GetFeatures().toast_controller();
  if (toast_controller) {
    ToastParams params = ToastParams(ToastId::kAddedToComparisonTable);

    params.body_string_replacement_params = {set_name};
    toast_controller->MaybeShowToast(ToastParams(std::move(params)));
  }
}

}  // namespace commerce
