// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_APP_SERVICE_INTERNALS_APP_SERVICE_INTERNALS_PAGE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_APP_SERVICE_INTERNALS_APP_SERVICE_INTERNALS_PAGE_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class AppServiceInternalsPageHandlerImpl
    : public mojom::app_service_internals::AppServiceInternalsPageHandler {
 public:
  explicit AppServiceInternalsPageHandlerImpl(
      Profile* profile,
      mojo::PendingReceiver<
          mojom::app_service_internals::AppServiceInternalsPageHandler>
          receiver);
  AppServiceInternalsPageHandlerImpl(
      const AppServiceInternalsPageHandlerImpl&) = delete;
  AppServiceInternalsPageHandlerImpl& operator=(
      const AppServiceInternalsPageHandlerImpl&) = delete;
  ~AppServiceInternalsPageHandlerImpl() override;

  // mojom::app_service_internals::AppServiceInternalsPageHandler:
  void GetDebugInfo(GetDebugInfoCallback callback) override;

 private:
  raw_ptr<Profile> profile_;
  mojo::Receiver<mojom::app_service_internals::AppServiceInternalsPageHandler>
      receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_APP_SERVICE_INTERNALS_APP_SERVICE_INTERNALS_PAGE_HANDLER_IMPL_H_
