// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NTP_PROMO_NTP_PROMO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NTP_PROMO_NTP_PROMO_HANDLER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom.h"
#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebContents;
}

class NtpPromoHandler : public ntp_promo::mojom::NtpPromoHandler {
 public:
  NtpPromoHandler(
      mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> pending_client,
      mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler> pending_handler,
      content::WebContents* web_contents);
  NtpPromoHandler(const NtpPromoHandler&) = delete;
  void operator=(const NtpPromoHandler&) = delete;
  ~NtpPromoHandler() override;

  // Returns the type of browser promo the New Tab Page in `web_contents` can
  // show: "simple", "empty" (promos are allowed but none is showable), or
  // "disabled" (no promo is allowed for the page).
  static std::string_view GetNtpPromoType(Profile* profile,
                                          content::WebContents* web_contents);

  // ntp_promo::mojom::NtpPromoHandler:
  void RequestPromos() override;
  void OnPromoClicked(const std::string& promo_id) override;
  void OnPromoDismissed(const std::string& promo_id) override;
  void OnPromoShown(const std::string& promo_id) override;

 protected:
  NtpPromoHandler(
      mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> pending_client,
      mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler> pending_handler,
      content::WebContents* web_contents,
      user_education::NtpPromoController* promo_controller);

  virtual user_education::UserEducationContextPtr GetContext() const;

 private:
  mojo::Remote<ntp_promo::mojom::NtpPromoClient> remote_client_;
  mojo::Receiver<ntp_promo::mojom::NtpPromoHandler> receiver_;
  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<user_education::NtpPromoController> promo_controller_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NTP_PROMO_NTP_PROMO_HANDLER_H_
