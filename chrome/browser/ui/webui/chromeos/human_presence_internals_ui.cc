// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/human_presence_internals_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chromeos/ash/components/human_presence/human_presence_configuration.h"
#include "chromeos/ash/components/human_presence/human_presence_internals.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "chromeos/dbus/human_presence/human_presence_dbus_client.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

// Class acting as a controller of the chrome://hps-internals WebUI.
class HumanPresenceInternalsUIMessageHandler
    : public content::WebUIMessageHandler,
      public chromeos::HumanPresenceDBusClient::Observer {
 public:
  HumanPresenceInternalsUIMessageHandler();

  HumanPresenceInternalsUIMessageHandler(
      const HumanPresenceInternalsUIMessageHandler&) = delete;
  HumanPresenceInternalsUIMessageHandler& operator=(
      const HumanPresenceInternalsUIMessageHandler&) = delete;

  ~HumanPresenceInternalsUIMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // chromeos::HumanPresenceDBusClient::Observer implementation.
  void OnHpsSenseChanged(const hps::HpsResultProto&) override;
  void OnHpsNotifyChanged(const hps::HpsResultProto&) override;
  void OnRestart() override;
  void OnShutdown() override;

 private:
  void Connect(const base::Value::List& args);
  void EnableLockOnLeave(const base::Value::List& args);
  void DisableLockOnLeave(const base::Value::List& args);
  void QueryLockOnLeave(const base::Value::List& args);
  void EnableSnoopingProtection(const base::Value::List& args);
  void DisableSnoopingProtection(const base::Value::List& args);
  void QuerySnoopingProtection(const base::Value::List& args);

  void OnConnected(bool connected);
  void OnLockOnLeaveResult(absl::optional<hps::HpsResultProto>);
  void OnSnoopingProtectionResult(absl::optional<hps::HpsResultProto>);
  static absl::optional<std::string> ReadManifest();
  void UpdateManifest(absl::optional<std::string> manifest);

  base::ScopedObservation<chromeos::HumanPresenceDBusClient,
                          chromeos::HumanPresenceDBusClient::Observer>
      human_presence_observation_{this};
  base::WeakPtrFactory<HumanPresenceInternalsUIMessageHandler>
      msg_weak_ptr_factory_{this};
  base::WeakPtrFactory<HumanPresenceInternalsUIMessageHandler>
      weak_ptr_factory_{this};
};

HumanPresenceInternalsUIMessageHandler::
    HumanPresenceInternalsUIMessageHandler() = default;

HumanPresenceInternalsUIMessageHandler::
    ~HumanPresenceInternalsUIMessageHandler() = default;

void HumanPresenceInternalsUIMessageHandler::OnHpsSenseChanged(
    const hps::HpsResultProto& state) {
  OnLockOnLeaveResult(state);
}

void HumanPresenceInternalsUIMessageHandler::OnHpsNotifyChanged(
    const hps::HpsResultProto& state) {
  OnSnoopingProtectionResult(state);
}

void HumanPresenceInternalsUIMessageHandler::OnLockOnLeaveResult(
    absl::optional<hps::HpsResultProto> state) {
  base::DictionaryValue value;
  if (state.has_value()) {
    value.SetInteger("state", state->value());
    value.SetInteger("inference_result", state->inference_result());
    value.SetInteger("inference_result_valid", state->inference_result_valid());
  } else {
    value.SetBoolean("disabled", true);
  }
  FireWebUIListener(hps::kHumanPresenceInternalsLockOnLeaveChangedEvent, value);
}

void HumanPresenceInternalsUIMessageHandler::OnSnoopingProtectionResult(
    absl::optional<hps::HpsResultProto> state) {
  base::DictionaryValue value;
  if (state.has_value()) {
    value.SetInteger("state", state->value());
    value.SetInteger("inference_result", state->inference_result());
    value.SetInteger("inference_result_valid", state->inference_result_valid());
  } else {
    value.SetBoolean("disabled", true);
  }
  FireWebUIListener(hps::kHumanPresenceInternalsSnoopingProtectionChangedEvent,
                    value);
}

void HumanPresenceInternalsUIMessageHandler::OnRestart() {
  OnConnected(true);
}

void HumanPresenceInternalsUIMessageHandler::OnShutdown() {
  OnConnected(false);
}

void HumanPresenceInternalsUIMessageHandler::Connect(
    const base::Value::List& args) {
  if (!chromeos::HumanPresenceDBusClient::Get()) {
    LOG(ERROR) << "HPS dbus client not available";
    return;
  }
  AllowJavascript();
  chromeos::HumanPresenceDBusClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&HumanPresenceInternalsUIMessageHandler::OnConnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HumanPresenceInternalsUIMessageHandler::OnConnected(bool connected) {
  base::DictionaryValue value;
  value.SetBoolean("connected", connected);
  FireWebUIListener(hps::kHumanPresenceInternalsConnectedEvent, value);

  if (connected) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&HumanPresenceInternalsUIMessageHandler::ReadManifest),
        base::BindOnce(&HumanPresenceInternalsUIMessageHandler::UpdateManifest,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

// static
absl::optional<std::string>
HumanPresenceInternalsUIMessageHandler::ReadManifest() {
  std::string manifest;
  const base::FilePath::CharType kManifestPath[] =
      FILE_PATH_LITERAL("/usr/lib/firmware/hps/manifest.txt");
  if (!base::ReadFileToString(base::FilePath(kManifestPath), &manifest))
    return absl::nullopt;
  return manifest;
}

void HumanPresenceInternalsUIMessageHandler::UpdateManifest(
    absl::optional<std::string> manifest) {
  if (!manifest.has_value()) {
    FireWebUIListener(hps::kHumanPresenceInternalsManifestEvent,
                      base::Value("(Failed to read manifest)"));
    return;
  }
  FireWebUIListener(hps::kHumanPresenceInternalsManifestEvent,
                    base::Value(*manifest));
}

void HumanPresenceInternalsUIMessageHandler::EnableLockOnLeave(
    const base::Value::List& args) {
  if (!chromeos::HumanPresenceDBusClient::Get() ||
      !hps::GetEnableLockOnLeaveConfig().has_value()) {
    FireWebUIListener(hps::kHumanPresenceInternalsEnableErrorEvent);
    return;
  }
  hps::FeatureConfig config(*hps::GetEnableLockOnLeaveConfig());
  config.set_report_raw_results(true);
  chromeos::HumanPresenceDBusClient::Get()->EnableHpsSense(config);
}

void HumanPresenceInternalsUIMessageHandler::DisableLockOnLeave(
    const base::Value::List& args) {
  if (chromeos::HumanPresenceDBusClient::Get())
    chromeos::HumanPresenceDBusClient::Get()->DisableHpsSense();
}

void HumanPresenceInternalsUIMessageHandler::QueryLockOnLeave(
    const base::Value::List& args) {
  if (!chromeos::HumanPresenceDBusClient::Get())
    return;
  chromeos::HumanPresenceDBusClient::Get()->GetResultHpsSense(base::BindOnce(
      &HumanPresenceInternalsUIMessageHandler::OnLockOnLeaveResult,
      weak_ptr_factory_.GetWeakPtr()));
}

void HumanPresenceInternalsUIMessageHandler::EnableSnoopingProtection(
    const base::Value::List& args) {
  if (!chromeos::HumanPresenceDBusClient::Get() ||
      !hps::GetEnableSnoopingProtectionConfig().has_value()) {
    FireWebUIListener(hps::kHumanPresenceInternalsEnableErrorEvent);
    return;
  }
  hps::FeatureConfig config(*hps::GetEnableSnoopingProtectionConfig());
  config.set_report_raw_results(true);
  chromeos::HumanPresenceDBusClient::Get()->EnableHpsNotify(config);
}

void HumanPresenceInternalsUIMessageHandler::DisableSnoopingProtection(
    const base::Value::List& args) {
  if (chromeos::HumanPresenceDBusClient::Get())
    chromeos::HumanPresenceDBusClient::Get()->DisableHpsNotify();
}

void HumanPresenceInternalsUIMessageHandler::QuerySnoopingProtection(
    const base::Value::List& args) {
  if (!chromeos::HumanPresenceDBusClient::Get())
    return;
  chromeos::HumanPresenceDBusClient::Get()->GetResultHpsNotify(base::BindOnce(
      &HumanPresenceInternalsUIMessageHandler::OnSnoopingProtectionResult,
      weak_ptr_factory_.GetWeakPtr()));
}

void HumanPresenceInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      hps::kHumanPresenceInternalsConnectCmd,
      base::BindRepeating(&HumanPresenceInternalsUIMessageHandler::Connect,
                          msg_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      hps::kHumanPresenceInternalsEnableLockOnLeaveCmd,
      base::BindRepeating(
          &HumanPresenceInternalsUIMessageHandler::EnableLockOnLeave,
          msg_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      hps::kHumanPresenceInternalsDisableLockOnLeaveCmd,
      base::BindRepeating(
          &HumanPresenceInternalsUIMessageHandler::DisableLockOnLeave,
          msg_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      hps::kHumanPresenceInternalsQueryLockOnLeaveCmd,
      base::BindRepeating(
          &HumanPresenceInternalsUIMessageHandler::QueryLockOnLeave,
          msg_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      hps::kHumanPresenceInternalsEnableSnoopingProtectionCmd,
      base::BindRepeating(
          &HumanPresenceInternalsUIMessageHandler::EnableSnoopingProtection,
          msg_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      hps::kHumanPresenceInternalsDisableSnoopingProtectionCmd,
      base::BindRepeating(
          &HumanPresenceInternalsUIMessageHandler::DisableSnoopingProtection,
          msg_weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      hps::kHumanPresenceInternalsQuerySnoopingProtectionCmd,
      base::BindRepeating(
          &HumanPresenceInternalsUIMessageHandler::QuerySnoopingProtection,
          msg_weak_ptr_factory_.GetWeakPtr()));
}

void HumanPresenceInternalsUIMessageHandler::OnJavascriptAllowed() {
  if (chromeos::HumanPresenceDBusClient::Get())
    human_presence_observation_.Observe(
        chromeos::HumanPresenceDBusClient::Get());
}

void HumanPresenceInternalsUIMessageHandler::OnJavascriptDisallowed() {
  // Invalidate weak ptrs in order to cancel any pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  human_presence_observation_.Reset();
}

}  // namespace

namespace chromeos {

HumanPresenceInternalsUI::HumanPresenceInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://gcm-internals source.
  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      chrome::kChromeUIHumanPresenceInternalsHost);

  html_source->UseStringsJs();

  // Add required resources.
  html_source->AddResourcePath(hps::kHumanPresenceInternalsCSS,
                               IDR_HUMAN_PRESENCE_INTERNALS_CSS);
  html_source->AddResourcePath(hps::kHumanPresenceInternalsJS,
                               IDR_HUMAN_PRESENCE_INTERNALS_JS);
  html_source->AddResourcePath(hps::kHumanPresenceInternalsIcon,
                               IDR_HUMAN_PRESENCE_INTERNALS_ICON);
  html_source->SetDefaultResource(IDR_HUMAN_PRESENCE_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);

  web_ui->AddMessageHandler(
      std::make_unique<HumanPresenceInternalsUIMessageHandler>());
}

HumanPresenceInternalsUI::~HumanPresenceInternalsUI() = default;

}  //  namespace chromeos
