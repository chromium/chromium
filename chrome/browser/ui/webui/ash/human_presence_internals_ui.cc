// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/human_presence_internals_ui.h"

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
#include "chromeos/ash/components/dbus/hps/hps_service.pb.h"
#include "chromeos/ash/components/dbus/human_presence/human_presence_dbus_client.h"
#include "chromeos/ash/components/human_presence/human_presence_configuration.h"
#include "chromeos/ash/components/human_presence/human_presence_internals.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

constexpr int kConsecutiveResultsFilterConfig = static_cast<int>(
    hps::FeatureConfig::FilterConfigCase::kConsecutiveResultsFilterConfig);
constexpr int kAverageFilterConfig = static_cast<int>(
    hps::FeatureConfig::FilterConfigCase::kAverageFilterConfig);

hps::FeatureConfig ParseFeatureConfigFromList(const base::Value::List& args) {
  hps::FeatureConfig config;

  // Check there is only one element in the list.
  if (args.size() != 1) {
    LOG(ERROR) << "HumanPresenceInternalsUIMessageHandler: Unexpected args "
                  "list with size "
               << args.size();
    return config;
  }

  // Check the only element is a JSON dictionary.
  const base::Value::Dict* arg0 = args[0].GetIfDict();
  if (!arg0) {
    LOG(ERROR) << "HumanPresenceInternalsUIMessageHandler: Unexpected arg0, "
                  "expecting a dictionary.";
    return config;
  }

  // Check that there is a valid filter_config_case in the map.
  absl::optional<int> filter_config_case = arg0->FindInt("filter_config_case");
  if (!filter_config_case.has_value() ||
      (filter_config_case.value() != kConsecutiveResultsFilterConfig &&
       filter_config_case.value() != kAverageFilterConfig)) {
    LOG(ERROR) << "HumanPresenceInternalsUIMessageHandler: Unexpected "
                  "filter_config_case.";
    return config;
  }

  // For the case of kConsecutiveResultsFilterConfig.
  if (filter_config_case.value() == kConsecutiveResultsFilterConfig) {
    auto& consecutive_results_filter_config =
        *config.mutable_consecutive_results_filter_config();
    consecutive_results_filter_config.set_positive_score_threshold(
        arg0->FindInt("positive_score_threshold").value_or(0));
    consecutive_results_filter_config.set_negative_score_threshold(
        arg0->FindInt("negative_score_threshold").value_or(0));
    consecutive_results_filter_config.set_positive_count_threshold(
        arg0->FindInt("positive_count_threshold").value_or(1));
    consecutive_results_filter_config.set_negative_count_threshold(
        arg0->FindInt("negative_count_threshold").value_or(1));
    consecutive_results_filter_config.set_uncertain_count_threshold(
        arg0->FindInt("uncertain_count_threshold").value_or(1));
    return config;
  }

  // For the case of kAverageFilterConfig.
  if (filter_config_case.value() == kAverageFilterConfig) {
    auto& average_filter_config = *config.mutable_average_filter_config();
    average_filter_config.set_average_window_size(
        arg0->FindInt("average_window_size").value_or(1));
    average_filter_config.set_positive_score_threshold(
        arg0->FindInt("positive_score_threshold").value_or(0));
    average_filter_config.set_negative_score_threshold(
        arg0->FindInt("negative_score_threshold").value_or(0));
    average_filter_config.set_default_uncertain_score(
        arg0->FindInt("default_uncertain_score").value_or(1));
    return config;
  }

  NOTREACHED();
  return config;
}

// Class acting as a controller of the chrome://hps-internals WebUI.
class HumanPresenceInternalsUIMessageHandler
    : public content::WebUIMessageHandler,
      public ash::HumanPresenceDBusClient::Observer {
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

  // ash::HumanPresenceDBusClient::Observer implementation.
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

  base::ScopedObservation<ash::HumanPresenceDBusClient,
                          ash::HumanPresenceDBusClient::Observer>
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
  base::Value::Dict value;
  if (state.has_value()) {
    value.Set("state", state->value());
    value.Set("inference_result", state->inference_result());
    value.Set("inference_result_valid", state->inference_result_valid());
  } else {
    value.Set("disabled", true);
  }
  FireWebUIListener(hps::kHumanPresenceInternalsLockOnLeaveChangedEvent, value);
}

void HumanPresenceInternalsUIMessageHandler::OnSnoopingProtectionResult(
    absl::optional<hps::HpsResultProto> state) {
  base::Value::Dict value;
  if (state.has_value()) {
    value.Set("state", state->value());
    value.Set("inference_result", state->inference_result());
    value.Set("inference_result_valid", state->inference_result_valid());
  } else {
    value.Set("disabled", true);
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
  if (!ash::HumanPresenceDBusClient::Get()) {
    LOG(ERROR) << "HumanPresenceInternalsUIMessageHandler: HPS dbus client not "
                  "available";
    return;
  }
  AllowJavascript();
  ash::HumanPresenceDBusClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&HumanPresenceInternalsUIMessageHandler::OnConnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HumanPresenceInternalsUIMessageHandler::OnConnected(bool connected) {
  base::Value::Dict value;
  value.Set("connected", connected);
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
  hps::FeatureConfig config;

  // If the args is empty, then try to get config from finch.
  if (args.empty()) {
    if (!ash::HumanPresenceDBusClient::Get() ||
        !hps::GetEnableLockOnLeaveConfig().has_value()) {
      FireWebUIListener(hps::kHumanPresenceInternalsEnableErrorEvent);
      return;
    }
    config = *hps::GetEnableLockOnLeaveConfig();
  } else {
    // Gets config from JSON list.
    config = ParseFeatureConfigFromList(args);
  }
  if (config.filter_config_case() ==
      hps::FeatureConfig::FilterConfigCase::FILTER_CONFIG_NOT_SET) {
    FireWebUIListener(hps::kHumanPresenceInternalsEnableErrorEvent);
    return;
  }
  config.set_report_raw_results(true);
  ash::HumanPresenceDBusClient::Get()->EnableHpsSense(config);
}

void HumanPresenceInternalsUIMessageHandler::DisableLockOnLeave(
    const base::Value::List& args) {
  if (ash::HumanPresenceDBusClient::Get())
    ash::HumanPresenceDBusClient::Get()->DisableHpsSense();
}

void HumanPresenceInternalsUIMessageHandler::QueryLockOnLeave(
    const base::Value::List& args) {
  if (!ash::HumanPresenceDBusClient::Get())
    return;
  ash::HumanPresenceDBusClient::Get()->GetResultHpsSense(base::BindOnce(
      &HumanPresenceInternalsUIMessageHandler::OnLockOnLeaveResult,
      weak_ptr_factory_.GetWeakPtr()));
}

void HumanPresenceInternalsUIMessageHandler::EnableSnoopingProtection(
    const base::Value::List& args) {
  hps::FeatureConfig config;

  // If the args is empty, then try to get config from finch.
  if (args.empty()) {
    if (!ash::HumanPresenceDBusClient::Get() ||
        !hps::GetEnableSnoopingProtectionConfig().has_value()) {
      FireWebUIListener(hps::kHumanPresenceInternalsEnableErrorEvent);
      return;
    }
    config = *hps::GetEnableSnoopingProtectionConfig();
  } else {
    // Gets config from JSON list.
    config = ParseFeatureConfigFromList(args);
  }
  if (config.filter_config_case() ==
      hps::FeatureConfig::FilterConfigCase::FILTER_CONFIG_NOT_SET) {
    FireWebUIListener(hps::kHumanPresenceInternalsEnableErrorEvent);
    return;
  }
  config.set_report_raw_results(true);
  ash::HumanPresenceDBusClient::Get()->EnableHpsNotify(config);
}

void HumanPresenceInternalsUIMessageHandler::DisableSnoopingProtection(
    const base::Value::List& args) {
  if (ash::HumanPresenceDBusClient::Get())
    ash::HumanPresenceDBusClient::Get()->DisableHpsNotify();
}

void HumanPresenceInternalsUIMessageHandler::QuerySnoopingProtection(
    const base::Value::List& args) {
  if (!ash::HumanPresenceDBusClient::Get())
    return;
  ash::HumanPresenceDBusClient::Get()->GetResultHpsNotify(base::BindOnce(
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
  if (ash::HumanPresenceDBusClient::Get())
    human_presence_observation_.Observe(ash::HumanPresenceDBusClient::Get());
}

void HumanPresenceInternalsUIMessageHandler::OnJavascriptDisallowed() {
  // Invalidate weak ptrs in order to cancel any pending callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  human_presence_observation_.Reset();
}

}  // namespace

namespace ash {

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

}  //  namespace ash
