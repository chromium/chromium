// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/product_specifications_ui_handler_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/webui/commerce/product_specifications_disclosure_dialog.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_ui.h"

namespace commerce {

ProductSpecificationsUIHandlerDelegate::ProductSpecificationsUIHandlerDelegate(
    content::WebUI* web_ui)
    : web_ui_(web_ui) {}

ProductSpecificationsUIHandlerDelegate::
    ~ProductSpecificationsUIHandlerDelegate() = default;

void ProductSpecificationsUIHandlerDelegate::ShowDisclosureDialog(
    const std::vector<GURL>& urls,
    const std::string& name,
    const std::string& set_id) {
  content::WebContents* web_contents = web_ui_->GetWebContents();
  if (!web_contents) {
    return;
  }
  // Currently this method is only used to trigger the dialog which will open
  // the potential product specification set in the current tab.
  DialogArgs dialog_args(urls, name, set_id, /*in_new_tab=*/false);
  ProductSpecificationsDisclosureDialog::ShowDialog(
      Profile::FromWebUI(web_ui_), web_contents, std::move(dialog_args));
}

void ProductSpecificationsUIHandlerDelegate::
    ShowProductSpecificationsSetForUuid(const base::Uuid& uuid,
                                        bool in_new_tab) {
  const GURL product_spec_url = commerce::GetProductSpecsTabUrlForID(uuid);
  auto* browser =
      chrome::FindLastActiveWithProfile(Profile::FromWebUI(web_ui_));
  if (!browser) {
    return;
  }
  if (in_new_tab) {
    content::OpenURLParams params(product_spec_url, content::Referrer(),
                                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                  ui::PAGE_TRANSITION_LINK, false);
    browser->OpenURL(params, /*navigation_handle_callback=*/{});
  } else {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (!web_contents) {
      return;
    }
    web_contents->GetController().LoadURL(product_spec_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                          /*extra_headers=*/std::string());
  }
}

void ProductSpecificationsUIHandlerDelegate::ShowSyncSetupFlow() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(Profile::FromWebUI(web_ui_));
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  signin_ui_util::EnableSyncFromSingleAccountPromo(
      Profile::FromWebUI(web_ui_), account_info,
      signin_metrics::AccessPoint::ACCESS_POINT_PRODUCT_SPECIFICATIONS);
}

}  // namespace commerce
