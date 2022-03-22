// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/hps_internals_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chromeos/components/hps/hps_configuration.h"
#include "chromeos/components/hps/hps_internals.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "chromeos/grit/chromeos_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

// Class acting as a controller of the chrome://hps-internals WebUI.
class HpsInternalsUIMessageHandler : public content::WebUIMessageHandler,
                                     public chromeos::HpsDBusClient::Observer {
 public:
  HpsInternalsUIMessageHandler();

  HpsInternalsUIMessageHandler(const HpsInternalsUIMessageHandler&) = delete;
  HpsInternalsUIMessageHandler& operator=(const HpsInternalsUIMessageHandler&) =
      delete;

  ~HpsInternalsUIMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // chromeos::HpsDBusClient::Observer implementation.
  void OnHpsSenseChanged(hps::HpsResult state) override;
  void OnHpsNotifyChanged(hps::HpsResult state) override;
  void OnRestart() override;
  void OnShutdown() override;

 private:
  void Connect(const base::Value::List& args);
  void EnableSense(const base::Value::List& args);
  void DisableSense(const base::Value::List& args);
  void QuerySense(const base::Value::List& args);
  void EnableNotify(const base::Value::List& args);
  void DisableNotify(const base::Value::List& args);
  void QueryNotify(const base::Value::List& args);

  void OnConnected(bool connected);
  void OnHpsSenseResult(absl::optional<hps::HpsResult>);
  void OnHpsNotifyResult(absl::optional<hps::HpsResult>);

  base::ScopedObservation<chromeos::HpsDBusClient,
                          chromeos::HpsDBusClient::Observer>
      hps_observation_{this};
  base::WeakPtrFactory<HpsInternalsUIMessageHandler> msg_weak_ptr_factory_{
      this};
  base::WeakPtrFactory<HpsInternalsUIMessageHandler> weak_ptr_factory_{this};
};

HpsInternalsUIMessageHandler::HpsInternalsUIMessageHandler() = default;

HpsInternalsUIMessageHandler::~HpsInternalsUIMessageHandler() = default;

void HpsInternalsUIMessageHandler::OnHpsSenseChanged(hps::HpsResult state) {
  OnHpsSenseResult(state);
}

void HpsInternalsUIMessageHandler::OnHpsNotifyChanged(hps::HpsResult state) {
  OnHpsNotifyResult(state);
}

void HpsInternalsUIMessageHandler::OnHpsSenseResult(
    absl::optional<hps::HpsResult> state) {
  base::DictionaryValue value;
  if (state.has_value()) {
    value.SetInteger("state", *state);
  } else {
    value.SetBoolean("disabled", true);
  }
  FireWebUIListener(hps::kHpsInternalsSenseChangedEvent, value);
}

void HpsInternalsUIMessageHandler::OnHpsNotifyResult(
    absl::optional<hps::HpsResult> state) {
  base::DictionaryValue value;
  if (state.has_value()) {
    value.SetInteger("state", *state);
  } else {
    value.SetBoolean("disabled", true);
  }
  FireWebUIListener(hps::kHpsInternalsNotifyChangedEvent, value);
}

void HpsInternalsUIMessageHandler::OnRestart() {
  OnConnected(true);
}

void HpsInternalsUIMessageHandler::OnShutdown() {
  OnConnected(false);
}

void HpsInternalsUIMessageHandler::Connect(const base::Value::List& args) {
  if (!chromeos::HpsDBusClient::Get()) {
    LOG(ERROR) << "HPS dbus client not available";
    return;
  }
  AllowJavascript();
  chromeos::HpsDBusClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&HpsInternalsUIMessageHandler::OnConnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HpsInternalsUIMessageHandler::OnConnected(bool connected) {
  base::DictionaryValue value;
  value.SetBoolean("connected", connected);
  FireWebUIListener(hps::kHpsInternalsConnectedEvent, value);
}

void HpsInternalsUIMessageHandler::EnableSense(const base::Value::List& args) {
  if (!chromeos::HpsDBusClient::Get() ||
      !hps::GetEnableHpsSenseConfig().has_value()) {
    FireWebUIListener(hps::kHpsInternalsEnableErrorEvent);
    return;
  }
  hps::FeatureConfig config(*hps::GetEnableHpsSenseConfig());
  chromeos::HpsDBusClient::Get()->EnableHpsSense(config);
}

void HpsInternalsUIMessageHandler::DisableSense(const base::Value::List& args) {
  if (chromeos::HpsDBusClient::Get())
    chromeos::HpsDBusClient::Get()->DisableHpsSense();
}

void HpsInternalsUIMessageHandler::QuerySense(const base::Value::List& args) {
  if (!chromeos::HpsDBusClient::Get())
    return;
  chromeos::HpsDBusClient::Get()->GetResultHpsSense(
      base::BindOnce(&HpsInternalsUIMessageHandler::OnHpsSenseResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HpsInternalsUIMessageHandler::EnableNotify(const base::Value::List& args) {
  if (!chromeos::HpsDBusClient::Get() ||
      !hps::GetEnableHpsNotifyConfig().has_value()) {
    FireWebUIListener(hps::kHpsInternalsEnableErrorEvent);
    return;
  }
  hps::FeatureConfig config(*hps::GetEnableHpsNotifyConfig());
  chromeos::HpsDBusClient::Get()->EnableHpsNotify(config);
}

void HpsInternalsUIMessageHandler::DisableNotify(
    const base::Value::List& args) {
  if (chromeos::HpsDBusClient::Get())
    chromeos::HpsDBusClient::Get()->DisableHpsNotify();
}

void HpsInternalsUIMessageHandler::QueryNotify(const base::Value::List& args) {
  if (!chromeos::HpsDBusClient::Get())
    return;
  chromeos::HpsDBusClient::Get()->GetResultHpsNotify(
      base::BindOnce(&HpsInternalsUIMessageHandler::OnHpsNotifyResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HpsInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      hps::kHpsInternalsConnectCmd,
      base::BindRepeating(&HpsInternalsUIMessageHandler::Connect,
                          msg_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      hps::kHpsInternalsEnableSenseCmd,
      base::BindRepeating(&HpsInternalsUIMessageHandler::EnableSense,
                          msg_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      hps::kHpsInternalsDisableSenseCmd,
      base::BindRepeating(&HpsInternalsUIMessageHandler::DisableSense,
                          msg_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      hps::kHpsInternalsQuerySenseCmd,
      base::BindRepeating(&HpsInternalsUIMessageHandler::QuerySense,
                          msg_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      hps::kHpsInternalsEnableNotifyCmd,
      base::BindRepeating(&HpsInternalsUIMessageHandler::EnableNotify,
                          msg_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      hps::kHpsInternalsDisableNotifyCmd,
      base::BindRepeating(&HpsInternalsUIMessageHandler::DisableNotify,
                          msg_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      hps::kHpsInternalsQueryNotifyCmd,
      base::BindRepeating(&HpsInternalsUIMessageHandler::QueryNotify,
                          msg_weak_ptr_factory_.GetWeakPtr()));
}

void HpsInternalsUIMessageHandler::OnJavascriptAllowed() {
  if (chromeos::HpsDBusClient::Get())
    hps_observation_.Observe(chromeos::HpsDBusClient::Get());
}

void HpsInternalsUIMessageHandler::OnJavascriptDisallowed() {
  // Invalidate weak ptrs in order to cancel any pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  hps_observation_.Reset();
}

}  // namespace

namespace chromeos {

HpsInternalsUI::HpsInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://gcm-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIHpsInternalsHost);

  html_source->UseStringsJs();

  // Add required resources.
  html_source->AddResourcePath(hps::kHpsInternalsCSS, IDR_HPS_INTERNALS_CSS);
  html_source->AddResourcePath(hps::kHpsInternalsJS, IDR_HPS_INTERNALS_JS);
  html_source->AddResourcePath(hps::kHpsInternalsIcon, IDR_HPS_INTERNALS_ICON);
  html_source->SetDefaultResource(IDR_HPS_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);

  web_ui->AddMessageHandler(std::make_unique<HpsInternalsUIMessageHandler>());
}

HpsInternalsUI::~HpsInternalsUI() = default;

}  //  namespace chromeos
