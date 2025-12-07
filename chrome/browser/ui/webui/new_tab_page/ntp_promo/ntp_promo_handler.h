// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NTP_PROMO_NTP_PROMO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NTP_PROMO_NTP_PROMO_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom.h"
#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}

class NtpPromoHandler : public ntp_promo::mojom::NtpPromoHandler {
 public:
  NtpPromoHandler(const NtpPromoHandler&) = delete;
  void operator=(const NtpPromoHandler&) = delete;
  ~NtpPromoHandler() override;

  // Convenience method so NTP doesn't have to care about User Education stuff
  // when creating the handler.
  static std::unique_ptr<NtpPromoHandler> Create(
      mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> pending_client,
      mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler> pending_handler,
      content::WebContents* web_contents);

  // Used for tests that want to directly inject a `promo_controller`.
  // Otherwise identical to `Create()`.
  static std::unique_ptr<NtpPromoHandler> CreateForTesting(
      mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> pending_client,
      mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler> pending_handler,
      const user_education::UserEducationContextPtr& ue_context,
      user_education::NtpPromoController* promo_controller);

  // ntp_promo::mojom::NtpPromoHandler:
  void RequestPromos() override;
  void OnPromoClicked(const std::string& promo_id) override;
  void OnPromosShown(const std::vector<std::string>& eligible_shown,
                     const std::vector<std::string>& completed_shown) override;
  void SnoozeSetupList() override;
  void UnsnoozeSetupList() override;
  void DisableSetupList() override;
  void UndisableSetupList() override;

 private:
  NtpPromoHandler(
      mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> pending_client,
      mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler> pending_handler,
      const user_education::UserEducationContextPtr& ue_context,
      user_education::NtpPromoController* promo_controller);

  mojo::Remote<ntp_promo::mojom::NtpPromoClient> remote_client_;
  mojo::Receiver<ntp_promo::mojom::NtpPromoHandler> receiver_;
  const user_education::UserEducationContextPtr ue_context_;
  const raw_ptr<user_education::NtpPromoController> promo_controller_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_NTP_PROMO_NTP_PROMO_HANDLER_H_
