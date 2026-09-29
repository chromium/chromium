// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_ui_platform_handlers.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/cr_components/history_clusters/history_clusters_util.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "components/user_education/webui/user_education.mojom.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/history/foreign_sessions.mojom.h"
#include "ui/webui/resources/cr_components/history/history_cross_device_signin_promo.mojom.h"

namespace history {

namespace {

// TODO(crbug.com/563034568): Enable foreign sessions on Android once
// ForeignSessionHandler is decoupled from desktop side-panel dependencies.
class AndroidForeignSessionHandler
    : public history::mojom::ForeignSessionPageHandler {
 public:
  AndroidForeignSessionHandler(
      mojo::PendingRemote<history::mojom::ForeignSessionPage> page,
      mojo::PendingReceiver<history::mojom::ForeignSessionPageHandler> receiver)
      : page_(std::move(page)), receiver_(this, std::move(receiver)) {}
  ~AndroidForeignSessionHandler() override = default;

  void GetForeignSessions(GetForeignSessionsCallback callback) override {
    std::move(callback).Run({});
  }
  void OpenForeignSessionAllTabs(const std::string& session_tag) override {}
  void OpenForeignSessionTab(const std::string& session_tag,
                             int32_t tab_id,
                             ui::mojom::ClickModifiersPtr modifiers) override {}
  void DeleteForeignSession(const std::string& session_tag) override {}
  void SetForeignSessionCollapsed(const std::string& session_tag,
                                  bool collapsed) override {}
  void ShowUi() override {}

 private:
  // Retained to prevent premature client-side disconnect callbacks.
  mojo::Remote<history::mojom::ForeignSessionPage> page_;
  mojo::Receiver<history::mojom::ForeignSessionPageHandler> receiver_;
};

// User education is a desktop-only concept that is not supported on Android.
class AndroidUserEducationMixedTrustHandler
    : public user_education::mojom::UserEducationMixedTrustHandler {
 public:
  explicit AndroidUserEducationMixedTrustHandler(
      mojo::PendingReceiver<
          user_education::mojom::UserEducationMixedTrustHandler> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~AndroidUserEducationMixedTrustHandler() override = default;

  void MaybeShowFeaturePromo(
      user_education::mojom::FeaturePromoParamsPtr params) override {}
  void NotifyFeaturePromoFeatureUsed(
      const std::string& feature_name,
      user_education::mojom::FeaturePromoFeatureUsedAction action) override {}
  void NotifyAdditionalConditionEvent(const std::string& event_name) override {}
  void NotifyNewBadgeFeatureUsed(const std::string& feature_name) override {}
  void MaybeShowNewBadgeFor(const std::string& feature_name,
                            MaybeShowNewBadgeForCallback callback) override {
    std::move(callback).Run(false);
  }

 private:
  mojo::Receiver<user_education::mojom::UserEducationMixedTrustHandler>
      receiver_;
};

// Cross-device sign-in promos are desktop-only concepts that are not supported
// on Android.
class AndroidHistoryCrossDeviceSigninPromoHandler
    : public history_cross_device_signin_promo::mojom::
          HistoryCrossDeviceSigninPromoHandler {
 public:
  explicit AndroidHistoryCrossDeviceSigninPromoHandler(
      mojo::PendingReceiver<history_cross_device_signin_promo::mojom::
                                HistoryCrossDeviceSigninPromoHandler> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~AndroidHistoryCrossDeviceSigninPromoHandler() override = default;

  void ShouldShowPromoCard(ShouldShowPromoCardCallback callback) override {
    std::move(callback).Run(false);
  }
  void OnPromoCardShown() override {}
  void OnPromoCardDismissed() override {}
  void OnPromoCardActionClicked(
      OnPromoCardActionClickedCallback callback) override {
    std::move(callback).Run();
  }

 private:
  mojo::Receiver<history_cross_device_signin_promo::mojom::
                     HistoryCrossDeviceSigninPromoHandler>
      receiver_;
};

}  // namespace

void PopulatePlatformDataSource(content::WebUIDataSource* source,
                                Profile* profile) {
  // Glic is desktop-only.
  source->AddBoolean(kIsGlicEnabledKey, false);
  source->AddBoolean(kIsGlicWebActuationAvailableKey, false);

  // Supply fallback empty strings to satisfy $i18n{} template replacement
  // dependencies in shared templates.
  source->AddString(kHistorySyncPromoBodySignedInKey, "");
  source->AddString(kHistorySyncPromoBodyWebOnlySignedInKey, "");
  source->AddString(kTurnOnSignedInSyncHistoryPromoBodySignInSyncOffKey, "");
  source->AddString(kSyncHistoryPromoBodyWebOnlySignedInKey, "");
  source->AddString(kTurnOnSyncButtonKey, "");

  // Managed UI is handled natively on Android.
  source->AddString(kManagedByIconKey, "");
  source->AddBoolean(kIsManagedKey, false);

  // History Clusters and Embeddings are disabled on Android.
  source->AddBoolean(kEnableHistoryEmbeddingsKey, false);
  source->AddBoolean(kMaybeShowEmbeddingsIphKey, false);
  source->AddBoolean(kIsHistoryClustersEnabledKey, false);
  source->AddBoolean(kIsHistoryClustersVisibleKey, false);
}

void InitializePlatformHandlers(content::WebUI* web_ui,
                                content::WebUIDataSource* /*source*/) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
  web_ui->RegisterMessageCallback(kObserveManagedUIMessage, base::DoNothing());
}

void InitializePlatformRegistrars(PrefChangeRegistrar& /*registrar*/,
                                  Profile* /*profile*/,
                                  base::RepeatingClosure /*update_callback*/) {}

void UpdatePlatformDataSource(content::WebUI* web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  base::DictValue update;
  update.Set(kIsHistoryClustersVisibleKey, false);

  content::WebUIDataSource::Update(profile, chrome::kChromeUIHistoryHost,
                                   std::move(update));
}

std::unique_ptr<history::mojom::ForeignSessionPageHandler>
CreateForeignSessionPageHandler(
    mojo::PendingRemote<history::mojom::ForeignSessionPage> page,
    mojo::PendingReceiver<history::mojom::ForeignSessionPageHandler> receiver,
    content::WebUI* web_ui) {
  return std::make_unique<AndroidForeignSessionHandler>(std::move(page),
                                                        std::move(receiver));
}

std::unique_ptr<user_education::mojom::UserEducationMixedTrustHandler>
CreateUserEducationMixedTrustHandler(
    mojo::PendingReceiver<user_education::mojom::UserEducationMixedTrustHandler>
        receiver,
    content::WebUI* web_ui) {
  return std::make_unique<AndroidUserEducationMixedTrustHandler>(
      std::move(receiver));
}

std::unique_ptr<history_cross_device_signin_promo::mojom::
                    HistoryCrossDeviceSigninPromoHandler>
CreateCrossDeviceSigninPromoHandler(
    mojo::PendingReceiver<history_cross_device_signin_promo::mojom::
                              HistoryCrossDeviceSigninPromoHandler> receiver,
    content::WebUI* web_ui) {
  return std::make_unique<AndroidHistoryCrossDeviceSigninPromoHandler>(
      std::move(receiver));
}

}  // namespace history
