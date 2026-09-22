// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_PLATFORM_HANDLERS_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_PLATFORM_HANDLERS_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "components/user_education/webui/user_education.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/webui/resources/cr_components/history/foreign_sessions.mojom-forward.h"
#include "ui/webui/resources/cr_components/history/history_cross_device_signin_promo.mojom-forward.h"

class PrefChangeRegistrar;
class Profile;

namespace content {
class WebUI;
class WebUIDataSource;
}  // namespace content

namespace history {

// Populates platform-specific WebUIDataSource strings and feature flags.
void PopulatePlatformDataSource(content::WebUIDataSource* source,
                                Profile* profile);

// Initializes platform-specific WebUI handlers and resources.
void InitializePlatformHandlers(content::WebUI* web_ui,
                                content::WebUIDataSource* source);

// Initializes platform-specific preference observation (e.g. History Clusters).
void InitializePlatformRegistrars(PrefChangeRegistrar& registrar,
                                  Profile* profile,
                                  base::RepeatingClosure update_callback);

// Updates platform-specific dynamic load-time data (e.g. clusters visibility).
void UpdatePlatformDataSource(content::WebUI* web_ui);

// Creates the platform-appropriate foreign session page handler.
std::unique_ptr<history::mojom::ForeignSessionPageHandler>
CreateForeignSessionPageHandler(
    mojo::PendingRemote<history::mojom::ForeignSessionPage> page,
    mojo::PendingReceiver<history::mojom::ForeignSessionPageHandler> receiver,
    content::WebUI* web_ui);

// Creates the platform-appropriate user education mixed trust handler.
std::unique_ptr<user_education::mojom::UserEducationMixedTrustHandler>
CreateUserEducationMixedTrustHandler(
    mojo::PendingReceiver<user_education::mojom::UserEducationMixedTrustHandler>
        receiver,
    content::WebUI* web_ui);

// Creates the platform-appropriate cross-device signin promo handler.
std::unique_ptr<history_cross_device_signin_promo::mojom::
                    HistoryCrossDeviceSigninPromoHandler>
CreateCrossDeviceSigninPromoHandler(
    mojo::PendingReceiver<history_cross_device_signin_promo::mojom::
                              HistoryCrossDeviceSigninPromoHandler> receiver,
    content::WebUI* web_ui);

}  // namespace history

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_PLATFORM_HANDLERS_H_
