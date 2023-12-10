// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromeos/standalone_browser_test_controller.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_crosapi_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/crosapi/mojom/tts.mojom-forward.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/tts_utterance.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "url/origin.h"

namespace {

blink::mojom::DisplayMode WindowModeToDisplayMode(
    apps::WindowMode window_mode) {
  switch (window_mode) {
    case apps::WindowMode::kBrowser:
      return blink::mojom::DisplayMode::kBrowser;
    case apps::WindowMode::kTabbedWindow:
      return blink::mojom::DisplayMode::kTabbed;
    case apps::WindowMode::kWindow:
      return blink::mojom::DisplayMode::kStandalone;
    case apps::WindowMode::kUnknown:
      return blink::mojom::DisplayMode::kUndefined;
  }
}

web_app::mojom::UserDisplayMode WindowModeToUserDisplayMode(
    apps::WindowMode window_mode) {
  switch (window_mode) {
    case apps::WindowMode::kBrowser:
      return web_app::mojom::UserDisplayMode::kBrowser;
    case apps::WindowMode::kTabbedWindow:
      return web_app::mojom::UserDisplayMode::kTabbed;
    case apps::WindowMode::kWindow:
      return web_app::mojom::UserDisplayMode::kStandalone;
    case apps::WindowMode::kUnknown:
      return web_app::mojom::UserDisplayMode::kBrowser;
  }
}

void IsolatedWebAppInstallationDone(
    const webapps::AppId& installed_app_id,
    StandaloneBrowserTestController::InstallIsolatedWebAppCallback callback,
    base::expected<web_app::InstallIsolatedWebAppCommandSuccess,
                   web_app::InstallIsolatedWebAppCommandError> install_result) {
  if (install_result.has_value()) {
    std::move(callback).Run(
        crosapi::mojom::InstallWebAppResult::NewAppId(installed_app_id));
  } else {
    std::move(callback).Run(
        crosapi::mojom::InstallWebAppResult::NewErrorMessage(
            install_result.error().message));
  }
}

void OnIsolatedWebAppUrlInfoCreated(
    const web_app::IsolatedWebAppLocation& iwa_location,
    StandaloneBrowserTestController::InstallIsolatedWebAppCallback callback,
    base::expected<web_app::IsolatedWebAppUrlInfo, std::string>
        iwa_url_info_expected) {
  ASSIGN_OR_RETURN(
      auto iwa_url_info, iwa_url_info_expected, [&](const std::string& error) {
        std::move(callback).Run(
            crosapi::mojom::InstallWebAppResult::NewErrorMessage(error));
      });

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  provider->scheduler().InstallIsolatedWebApp(
      iwa_url_info, iwa_location, /*expected_version=*/absl::nullopt,
      /*optional_keep_alive=*/nullptr, /*optional_profile_keep_alive=*/nullptr,
      base::BindOnce(&IsolatedWebAppInstallationDone, iwa_url_info.app_id(),
                     std::move(callback)));
}

}  // namespace

// With Lacros tts support enabled, all Lacros utterances will be sent to
// Ash to be processed by TtsController in Ash. When the utterance is spoken
// by a speech engine (provided by Ash or Lacros), we need to make sure that
// Tts events are routed back to the UtteranceEventDelegate in Lacros.
// This class can be set as UtteranceEventDelegate for Lacros Utterance used
// for testing.
class StandaloneBrowserTestController::LacrosUtteranceEventDelegate
    : public content::UtteranceEventDelegate {
 public:
  LacrosUtteranceEventDelegate(
      StandaloneBrowserTestController* controller,
      mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient> client)
      : controller_(controller), client_(std::move(client)) {}

  LacrosUtteranceEventDelegate(const LacrosUtteranceEventDelegate&) = delete;
  LacrosUtteranceEventDelegate& operator=(const LacrosUtteranceEventDelegate&) =
      delete;
  ~LacrosUtteranceEventDelegate() override = default;

  // content::UtteranceEventDelegate methods:
  void OnTtsEvent(content::TtsUtterance* utterance,
                  content::TtsEventType event_type,
                  int char_index,
                  int char_length,
                  const std::string& error_message) override {
    // Forward the TtsEvent back to ash, so that ash browser test can verify
    // that TtsEvent has been routed to the UtteranceEventDelegate in Lacros.
    client_->OnTtsEvent(tts_crosapi_util::ToMojo(event_type), char_index,
                        char_length, error_message);

    if (utterance->IsFinished())
      controller_->OnUtteranceFinished(utterance->GetId());
    // Note: |this| is deleted if utterance->IsFinished().
  }

 private:
  // |controller_| is guaranteed to be valid during the lifetime of this class.
  const raw_ptr<StandaloneBrowserTestController> controller_;
  mojo::Remote<crosapi::mojom::TtsUtteranceClient> client_;
};

StandaloneBrowserTestController::StandaloneBrowserTestController(
    mojo::Remote<crosapi::mojom::TestController>& test_controller) {
  CHECK_IS_TEST();
  test_controller->RegisterStandaloneBrowserTestController(
      controller_receiver_.BindNewPipeAndPassRemoteWithVersion());
  test_controller.FlushAsync();
}

StandaloneBrowserTestController::~StandaloneBrowserTestController() = default;

void StandaloneBrowserTestController::InstallWebApp(
    const std::string& start_url,
    apps::WindowMode window_mode,
    InstallWebAppCallback callback) {
  auto info = std::make_unique<web_app::WebAppInstallInfo>();
  info->title = u"Test Web App";
  info->start_url = GURL(start_url);
  info->display_mode = WindowModeToDisplayMode(window_mode);
  info->user_display_mode = WindowModeToUserDisplayMode(window_mode);
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  provider->scheduler().InstallFromInfo(
      std::move(info),
      /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::SYNC,
      base::BindOnce(&StandaloneBrowserTestController::WebAppInstallationDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void StandaloneBrowserTestController::InstallUnpackedExtension(
    const std::string& path,
    InstallUnpackedExtensionCallback callback) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  extensions::ChromeTestExtensionLoader loader(profile);
  loader.LoadUnpackedExtensionAsync(
      base::FilePath{path},
      base::BindOnce([](const extensions::Extension* extension) {
        return extension->id();
      }).Then(std::move(callback)));
}

void StandaloneBrowserTestController::ObserveDomMessages(
    mojo::PendingRemote<crosapi::mojom::DomMessageObserver> observer,
    ObserveDomMessagesCallback callback) {
  dom_message_observer_.Bind(std::move(observer));
  dom_message_observer_.set_disconnect_handler(base::BindOnce(
      &StandaloneBrowserTestController::OnDomMessageObserverDisconnected,
      weak_ptr_factory_.GetWeakPtr()));

  ASSERT_FALSE(dom_message_queue_.has_value());
  dom_message_queue_.emplace();
  dom_message_queue_->SetOnMessageAvailableCallback(
      base::BindOnce(&StandaloneBrowserTestController::OnDomMessageQueueReady,
                     weak_ptr_factory_.GetWeakPtr()));

  std::move(callback).Run();
}

void StandaloneBrowserTestController::OnDomMessageObserverDisconnected() {
  dom_message_queue_.reset();
  dom_message_observer_.reset();
}

void StandaloneBrowserTestController::OnDomMessageQueueReady() {
  std::string message;
  ASSERT_TRUE(dom_message_queue_->PopMessage(&message));
  dom_message_observer_->OnMessage(message);

  dom_message_queue_->SetOnMessageAvailableCallback(
      base::BindOnce(&StandaloneBrowserTestController::OnDomMessageQueueReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void StandaloneBrowserTestController::RemoveComponentExtension(
    const std::string& extension_id,
    RemoveComponentExtensionCallback callback) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  extensions::ExtensionSystem::Get(profile)
      ->extension_service()
      ->RemoveComponentExtension(extension_id);
  std::move(callback).Run();
}

void StandaloneBrowserTestController::LoadVpnExtension(
    const std::string& extension_name,
    LoadVpnExtensionCallback callback) {
  std::string error;
  auto extension = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kUnpacked,
      CreateVpnExtensionManifest(extension_name),
      extensions::Extension::NO_FLAGS, &error);
  if (!error.empty()) {
    std::move(callback).Run(error);
    return;
  }

  auto* extension_registry = extensions::ExtensionRegistry::Get(
      ProfileManager::GetPrimaryUserProfile());
  extension_registry->AddEnabled(extension);
  extension_registry->TriggerOnLoaded(extension.get());

  std::move(callback).Run(extension->id());
}

void StandaloneBrowserTestController::GetTtsVoices(
    GetTtsVoicesCallback callback) {
  std::vector<content::VoiceData> voices;
  tts_crosapi_util::GetAllVoicesForTesting(  // IN-TEST
      ProfileManager::GetActiveUserProfile(), GURL(), &voices);

  std::vector<crosapi::mojom::TtsVoicePtr> mojo_voices;
  for (const auto& voice : voices)
    mojo_voices.push_back(tts_crosapi_util::ToMojo(voice));

  std::move(callback).Run(std::move(mojo_voices));
}

void StandaloneBrowserTestController::TtsSpeak(
    crosapi::mojom::TtsUtterancePtr mojo_utterance,
    mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient> utterance_client) {
  std::unique_ptr<content::TtsUtterance> lacros_utterance =
      tts_crosapi_util::CreateUtteranceFromMojo(
          mojo_utterance, /*should_always_be_spoken=*/true);
  auto event_delegate = std::make_unique<LacrosUtteranceEventDelegate>(
      this, std::move(utterance_client));
  lacros_utterance->SetEventDelegate(event_delegate.get());
  lacros_utterance_event_delegates_.emplace(lacros_utterance->GetId(),
                                            std ::move(event_delegate));
  tts_crosapi_util::SpeakForTesting(std::move(lacros_utterance));
}

void StandaloneBrowserTestController::InstallSubApp(
    const webapps::AppId& parent_app_id,
    const std::string& sub_app_path,
    InstallSubAppCallback callback) {
  GURL parent_app_url(
      base::StrCat({chrome::kIsolatedAppScheme, url::kStandardSchemeSeparator,
                    parent_app_id}));

  auto info = std::make_unique<web_app::WebAppInstallInfo>();
  info->start_url = parent_app_url.Resolve(sub_app_path);
  info->parent_app_id = parent_app_id;
  info->parent_app_manifest_id = parent_app_url;
  info->title = u"Sub App";

  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  provider->scheduler().InstallFromInfo(
      std::move(info),
      /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::SUB_APP,
      base::BindOnce(&StandaloneBrowserTestController::WebAppInstallationDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void StandaloneBrowserTestController::InstallIsolatedWebApp(
    crosapi::mojom::IsolatedWebAppLocationPtr location,
    bool dev_mode,
    InstallIsolatedWebAppCallback callback) {
  web_app::IsolatedWebAppLocation iwa_location_location;
  if (dev_mode) {
    if (location->is_bundle_path()) {
      iwa_location_location = web_app::DevModeBundle{
          .path = base::FilePath(location->get_bundle_path())};
    } else {
      iwa_location_location = web_app::DevModeProxy{
          .proxy_url = url::Origin::Create(location->get_proxy_origin())};
    }
  } else {
    iwa_location_location = web_app::InstalledBundle{
        .path = base::FilePath(location->get_bundle_path())};
  }

  web_app::IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppLocation(
      iwa_location_location,
      base::BindOnce(&OnIsolatedWebAppUrlInfoCreated, iwa_location_location,
                     std::move(callback)));
}

void StandaloneBrowserTestController::SetWebAppSettingsPref(
    const std::string& policy,
    SetWebAppSettingsPrefCallback callback) {
  CHECK(callback);

  auto result = base::JSONReader::ReadAndReturnValueWithError(
      policy, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  if (!result.has_value()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  if (!result->is_list()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  ProfileManager::GetPrimaryUserProfile()->GetPrefs()->SetList(
      prefs::kWebAppSettings, std::move(*result).TakeList());

  std::move(callback).Run(/*success=*/true);
}

void StandaloneBrowserTestController::OnUtteranceFinished(int utterance_id) {
  // Delete the utterace event delegate object when the utterance is finished.
  lacros_utterance_event_delegates_.erase(utterance_id);
}

void StandaloneBrowserTestController::GetExtensionKeeplist(
    GetExtensionKeeplistCallback callback) {
  auto mojo_keeplist = crosapi::mojom::ExtensionKeepList::New();

  for (const auto& id :
       extensions::GetExtensionsRunInOSAndStandaloneBrowser()) {
    mojo_keeplist->extensions_run_in_os_and_standalonebrowser.push_back(
        std::string(id));
  }

  for (const auto& id :
       extensions::GetExtensionAppsRunInOSAndStandaloneBrowser()) {
    mojo_keeplist->extension_apps_run_in_os_and_standalonebrowser.push_back(
        std::string(id));
  }

  for (const auto& id : extensions::GetExtensionsRunInOSOnly())
    mojo_keeplist->extensions_run_in_os_only.push_back(std::string(id));

  for (const auto& id : extensions::GetExtensionAppsRunInOSOnly())
    mojo_keeplist->extension_apps_run_in_os_only.push_back(std::string(id));

  std::move(callback).Run(std::move(mojo_keeplist));
}

void StandaloneBrowserTestController::WebAppInstallationDone(
    InstallWebAppCallback callback,
    const webapps::AppId& installed_app_id,
    webapps::InstallResultCode code) {
  std::move(callback).Run(code == webapps::InstallResultCode::kSuccessNewInstall
                              ? installed_app_id
                              : "");
}

base::Value::Dict StandaloneBrowserTestController::CreateVpnExtensionManifest(
    const std::string& extension_name) {
  base::Value::Dict manifest;

  manifest.Set(extensions::manifest_keys::kName, extension_name);
  manifest.Set(extensions::manifest_keys::kVersion, "1.0");
  manifest.Set(extensions::manifest_keys::kManifestVersion, 2);

  base::Value::List permissions;
  permissions.Append("vpnProvider");
  manifest.Set(extensions::manifest_keys::kPermissions, std::move(permissions));

  return manifest;
}
