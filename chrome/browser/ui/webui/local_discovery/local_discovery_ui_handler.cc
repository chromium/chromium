// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/browser/printing/cloud_print/privet_confirm_api_flow.h"
#include "chrome/browser/printing/cloud_print/privet_constants.h"
#include "chrome/browser/printing/cloud_print/privet_device_lister_impl.h"
#include "chrome/browser/printing/cloud_print/privet_http_asynchronous_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OS_CHROMEOS)
#define CLOUD_PRINT_CONNECTOR_UI_AVAILABLE
#endif

#if defined(CLOUD_PRINT_CONNECTOR_UI_AVAILABLE)
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#endif

using cloud_print::CloudPrintPrinterList;
using cloud_print::DeviceDescription;
using cloud_print::GCDApiFlow;
using cloud_print::PrivetRegisterOperation;

namespace local_discovery {

namespace {

const char kDictionaryKeyServiceName[] = "service_name";
const char kDictionaryKeyDisplayName[] = "display_name";
const char kDictionaryKeyDescription[] = "description";
const char kDictionaryKeyID[] = "id";

const char kKeyPrefixMDns[] = "MDns:";

int g_num_visible = 0;

const int kCloudDevicesPrivetVersion = 3;

std::unique_ptr<base::DictionaryValue> CreateDeviceInfo(
    const CloudPrintPrinterList::Device& description) {
  std::unique_ptr<base::DictionaryValue> return_value(
      new base::DictionaryValue);

  return_value->SetString(kDictionaryKeyID, description.id);
  return_value->SetString(kDictionaryKeyDisplayName, description.display_name);
  return_value->SetString(kDictionaryKeyDescription, description.description);

  return return_value;
}

void ReadDevicesList(const CloudPrintPrinterList::DeviceList& devices,
                     const std::set<std::string>& local_ids,
                     base::ListValue* devices_list) {
  for (const auto& i : devices) {
    if (base::Contains(local_ids, i.id)) {
      devices_list->Append(CreateDeviceInfo(i));
    }
  }

  for (const auto& i : devices) {
    if (!base::Contains(local_ids, i.id)) {
      devices_list->Append(CreateDeviceInfo(i));
    }
  }
}

scoped_refptr<network::SharedURLLoaderFactory>&
GetURLLoaderFactoryForTesting() {
  static base::NoDestructor<scoped_refptr<network::SharedURLLoaderFactory>>
      instance;
  return *instance;
}

}  // namespace

LocalDiscoveryUIHandler::SetURLLoaderFactoryForTesting::
    SetURLLoaderFactoryForTesting(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK(!GetURLLoaderFactoryForTesting());
  GetURLLoaderFactoryForTesting() = url_loader_factory;
}

LocalDiscoveryUIHandler::SetURLLoaderFactoryForTesting::
    ~SetURLLoaderFactoryForTesting() {
  GetURLLoaderFactoryForTesting() = nullptr;
}

LocalDiscoveryUIHandler::LocalDiscoveryUIHandler()
    : failed_list_count_(0), succeded_list_count_(0) {
  g_num_visible++;
}

LocalDiscoveryUIHandler::~LocalDiscoveryUIHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (identity_manager)
    identity_manager->RemoveObserver(this);
  ResetCurrentRegistration();
  g_num_visible--;
}

// static
bool LocalDiscoveryUIHandler::GetHasVisible() {
  return g_num_visible != 0;
}

void LocalDiscoveryUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "start", base::BindRepeating(&LocalDiscoveryUIHandler::HandleStart,
                                   base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "registerDevice",
      base::BindRepeating(&LocalDiscoveryUIHandler::HandleRegisterDevice,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancelRegistration",
      base::BindRepeating(&LocalDiscoveryUIHandler::HandleCancelRegistration,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestDeviceList",
      base::BindRepeating(&LocalDiscoveryUIHandler::HandleRequestDeviceList,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openCloudPrintURL",
      base::BindRepeating(&LocalDiscoveryUIHandler::HandleOpenCloudPrintURL,
                          base::Unretained(this)));
#if !defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "showSyncUI",
      base::BindRepeating(&LocalDiscoveryUIHandler::HandleShowSyncUI,
                          base::Unretained(this)));
#endif

  // Cloud print connector related messages
#if defined(CLOUD_PRINT_CONNECTOR_UI_AVAILABLE)
  web_ui()->RegisterMessageCallback(
      "showCloudPrintSetupDialog",
      base::BindRepeating(&LocalDiscoveryUIHandler::ShowCloudPrintSetupDialog,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "disableCloudPrintConnector",
      base::BindRepeating(
          &LocalDiscoveryUIHandler::HandleDisableCloudPrintConnector,
          base::Unretained(this)));
#endif  // defined(CLOUD_PRINT_CONNECTOR_UI_AVAILABLE)
}

void LocalDiscoveryUIHandler::HandleStart(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());

  // If |privet_lister_| is already set, it is a mock used for tests or the
  // result of a reload.
  if (!privet_lister_) {
    service_discovery_client_ = ServiceDiscoverySharedClient::GetInstance();
    privet_lister_.reset(
        new cloud_print::PrivetDeviceListerImpl(service_discovery_client_.get(),
                                                this));

    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        GetURLLoaderFactoryForTesting();
    if (!url_loader_factory)
      url_loader_factory = profile->GetURLLoaderFactory();

    privet_http_factory_ =
        cloud_print::PrivetHTTPAsynchronousFactory::CreateInstance(
            url_loader_factory);

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    if (identity_manager)
      identity_manager->AddObserver(this);
  }

  privet_lister_->Start();
  privet_lister_->DiscoverNewDevices();

#if defined(CLOUD_PRINT_CONNECTOR_UI_AVAILABLE)
  StartCloudPrintConnector();
#endif

  CheckUserLoggedIn();
}

void LocalDiscoveryUIHandler::HandleRegisterDevice(
    const base::ListValue* args) {
  std::string device;
  bool rv = args->GetString(0, &device);
  DCHECK(rv);

  auto it = device_descriptions_.find(device);
  if (it == device_descriptions_.end()) {
    OnSetupError();
    return;
  }

  if (it->second.version < kCloudDevicesPrivetVersion) {
    privet_resolution_ = privet_http_factory_->CreatePrivetHTTP(device);
    privet_resolution_->Start(
        it->second.address,
        base::Bind(&LocalDiscoveryUIHandler::StartRegisterHTTP,
                   base::Unretained(this)));
  } else {
    OnSetupError();
  }
}

void LocalDiscoveryUIHandler::HandleCancelRegistration(
    const base::ListValue* args) {
  ResetCurrentRegistration();
}

void LocalDiscoveryUIHandler::HandleRequestDeviceList(
    const base::ListValue* args) {
  failed_list_count_ = 0;
  succeded_list_count_ = 0;
  cloud_devices_.clear();

  cloud_print_printer_list_ = CreateApiFlow();

  if (cloud_print_printer_list_) {
    cloud_print_printer_list_->Start(
        std::make_unique<CloudPrintPrinterList>(this));
  }

  CheckListingDone();
}

void LocalDiscoveryUIHandler::HandleOpenCloudPrintURL(
    const base::ListValue* args) {
  std::string id;
  bool rv = args->GetString(0, &id);
  DCHECK(rv);

  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  DCHECK(browser);

  chrome::AddSelectedTabWithURL(browser,
                                cloud_devices::GetCloudPrintManageDeviceURL(id),
                                ui::PAGE_TRANSITION_FROM_API);
}

#if !defined(OS_CHROMEOS)
void LocalDiscoveryUIHandler::HandleShowSyncUI(
    const base::ListValue* args) {
  Browser* browser = chrome::FindBrowserWithWebContents(
      web_ui()->GetWebContents());
  DCHECK(browser);
  chrome::ShowBrowserSignin(
      browser, signin_metrics::AccessPoint::ACCESS_POINT_DEVICES_PAGE);
}
#endif

void LocalDiscoveryUIHandler::StartRegisterHTTP(
    std::unique_ptr<cloud_print::PrivetHTTPClient> http_client) {
  current_http_client_ =
      cloud_print::PrivetV1HTTPClient::CreateDefault(std::move(http_client));

  if (!current_http_client_) {
    SendRegisterError();
    return;
  }

  current_register_operation_ =
      current_http_client_->CreateRegisterOperation(GetSyncAccount(), this);
  current_register_operation_->Start();
}

void LocalDiscoveryUIHandler::OnPrivetRegisterClaimToken(
    PrivetRegisterOperation* operation,
    const std::string& token,
    const GURL& url) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "local_discovery.onRegistrationConfirmedOnPrinter");
  if (!base::Contains(device_descriptions_, current_http_client_->GetName())) {
    SendRegisterError();
    return;
  }

  confirm_api_call_flow_ = CreateApiFlow();
  if (!confirm_api_call_flow_) {
    SendRegisterError();
    return;
  }
  confirm_api_call_flow_->Start(
      std::make_unique<cloud_print::PrivetConfirmApiCallFlow>(
          token, base::Bind(&LocalDiscoveryUIHandler::OnConfirmDone,
                            base::Unretained(this))));
}

void LocalDiscoveryUIHandler::OnPrivetRegisterError(
    PrivetRegisterOperation* operation,
    const std::string& action,
    PrivetRegisterOperation::FailureReason reason,
    int printer_http_code,
    const base::DictionaryValue* json) {
  std::string error;
  if (reason == PrivetRegisterOperation::FAILURE_JSON_ERROR &&
      json->GetString(cloud_print::kPrivetKeyError, &error)) {
    if (error == cloud_print::kPrivetErrorTimeout) {
      web_ui()->CallJavascriptFunctionUnsafe(
          "local_discovery.onRegistrationTimeout");
      return;
    }
    if (error == cloud_print::kPrivetErrorCancel) {
      web_ui()->CallJavascriptFunctionUnsafe(
          "local_discovery.onRegistrationCanceledPrinter");
      return;
    }
  }

  SendRegisterError();
}

void LocalDiscoveryUIHandler::OnPrivetRegisterDone(
    PrivetRegisterOperation* operation,
    const std::string& device_id) {
  std::string name = operation->GetHTTPClient()->GetName();
  current_register_operation_.reset();
  current_http_client_.reset();
  SendRegisterDone(name);
}

void LocalDiscoveryUIHandler::OnSetupError() {
  ResetCurrentRegistration();
  SendRegisterError();
}

void LocalDiscoveryUIHandler::OnConfirmDone(GCDApiFlow::Status status) {
  if (status == GCDApiFlow::SUCCESS) {
    confirm_api_call_flow_.reset();
    current_register_operation_->CompleteRegistration();
  } else {
    SendRegisterError();
  }
}

void LocalDiscoveryUIHandler::DeviceChanged(
    const std::string& name,
    const DeviceDescription& description) {
  device_descriptions_[name] = description;

  base::DictionaryValue info;

  base::Value service_key(kKeyPrefixMDns + name);

  if (description.id.empty()) {
    info.SetString(kDictionaryKeyServiceName, name);
    info.SetString(kDictionaryKeyDisplayName, description.name);
    info.SetString(kDictionaryKeyDescription, description.description);

    web_ui()->CallJavascriptFunctionUnsafe(
        "local_discovery.onUnregisteredDeviceUpdate", service_key, info);
  } else {
    auto null_value = std::make_unique<base::Value>();

    web_ui()->CallJavascriptFunctionUnsafe(
        "local_discovery.onUnregisteredDeviceUpdate", service_key, *null_value);
  }
}

void LocalDiscoveryUIHandler::DeviceRemoved(const std::string& name) {
  device_descriptions_.erase(name);
  auto null_value = std::make_unique<base::Value>();
  base::Value name_value(kKeyPrefixMDns + name);

  web_ui()->CallJavascriptFunctionUnsafe(
      "local_discovery.onUnregisteredDeviceUpdate", name_value, *null_value);
}

void LocalDiscoveryUIHandler::DeviceCacheFlushed() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "local_discovery.onDeviceCacheFlushed");
  privet_lister_->DiscoverNewDevices();
}

void LocalDiscoveryUIHandler::OnDeviceListReady(
    const CloudPrintPrinterList::DeviceList& devices) {
  cloud_devices_.insert(cloud_devices_.end(), devices.begin(), devices.end());
  ++succeded_list_count_;
  CheckListingDone();
}

void LocalDiscoveryUIHandler::OnDeviceListUnavailable() {
  ++failed_list_count_;
  CheckListingDone();
}

void LocalDiscoveryUIHandler::OnPrimaryAccountSet(
    const CoreAccountInfo& primary_account_info) {
  CheckUserLoggedIn();
}

void LocalDiscoveryUIHandler::OnPrimaryAccountCleared(
    const CoreAccountInfo& previous_primary_account_info) {
  CheckUserLoggedIn();
}

void LocalDiscoveryUIHandler::SendRegisterError() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "local_discovery.onRegistrationFailed");
}

void LocalDiscoveryUIHandler::SendRegisterDone(
    const std::string& service_name) {
  // HACK(noamsml): Generate network traffic so the Windows firewall doesn't
  // block the printer's announcement.
  privet_lister_->DiscoverNewDevices();

  auto it = device_descriptions_.find(service_name);

  if (it == device_descriptions_.end()) {
    // TODO(noamsml): Handle the case where a printer's record is not present at
    // the end of registration.
    SendRegisterError();
    return;
  }

  const DeviceDescription& device = it->second;
  base::DictionaryValue device_value;

  device_value.SetString(kDictionaryKeyID, device.id);
  device_value.SetString(kDictionaryKeyDisplayName, device.name);
  device_value.SetString(kDictionaryKeyDescription, device.description);
  device_value.SetString(kDictionaryKeyServiceName, service_name);

  web_ui()->CallJavascriptFunctionUnsafe(
      "local_discovery.onRegistrationSuccess", device_value);
}

std::string LocalDiscoveryUIHandler::GetSyncAccount() const {
  Profile* profile = Profile::FromWebUI(web_ui());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  std::string email;
  if (identity_manager && identity_manager->HasPrimaryAccount())
    email = identity_manager->GetPrimaryAccountInfo().email;
  return email;
}

// TODO(noamsml): Create master object for registration flow.
void LocalDiscoveryUIHandler::ResetCurrentRegistration() {
  if (current_register_operation_) {
    current_register_operation_->Cancel();
    current_register_operation_.reset();
  }

  confirm_api_call_flow_.reset();
  privet_resolution_.reset();
  current_http_client_.reset();
}

void LocalDiscoveryUIHandler::CheckUserLoggedIn() {
  base::Value logged_in_value(!GetSyncAccount().empty());
  base::Value is_supervised_value(IsUserSupervisedOrOffTheRecord());
  web_ui()->CallJavascriptFunctionUnsafe("local_discovery.setUserLoggedIn",
                                         logged_in_value, is_supervised_value);
}

void LocalDiscoveryUIHandler::CheckListingDone() {
  int started = cloud_print_printer_list_ ? 1 : 0;
  if (started > failed_list_count_ + succeded_list_count_)
    return;

  if (succeded_list_count_ <= 0) {
    web_ui()->CallJavascriptFunctionUnsafe(
        "local_discovery.onCloudDeviceListUnavailable");
    return;
  }

  base::ListValue devices_list;
  std::set<std::string> local_ids;

  for (const auto& it : device_descriptions_)
    local_ids.insert(it.second.id);

  ReadDevicesList(cloud_devices_, local_ids, &devices_list);

  web_ui()->CallJavascriptFunctionUnsafe(
      "local_discovery.onCloudDeviceListAvailable", devices_list);
  cloud_print_printer_list_.reset();
}

std::unique_ptr<GCDApiFlow> LocalDiscoveryUIHandler::CreateApiFlow() {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return std::unique_ptr<GCDApiFlow>();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!(identity_manager && identity_manager->HasPrimaryAccount()))
    return std::unique_ptr<GCDApiFlow>();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      GetURLLoaderFactoryForTesting();
  if (!url_loader_factory)
    url_loader_factory = profile->GetURLLoaderFactory();

  return GCDApiFlow::Create(url_loader_factory, identity_manager);
}

bool LocalDiscoveryUIHandler::IsUserSupervisedOrOffTheRecord() {
  Profile* profile = Profile::FromWebUI(web_ui());
  return profile->IsSupervised() || profile->IsOffTheRecord();
}

#if defined(CLOUD_PRINT_CONNECTOR_UI_AVAILABLE)
void LocalDiscoveryUIHandler::StartCloudPrintConnector() {
  Profile* profile = Profile::FromWebUI(web_ui());

  base::Closure cloud_print_callback = base::Bind(
      &LocalDiscoveryUIHandler::OnCloudPrintPrefsChanged,
          base::Unretained(this));

  if (cloud_print_connector_email_.GetPrefName().empty()) {
    cloud_print_connector_email_.Init(
        prefs::kCloudPrintEmail, profile->GetPrefs(), cloud_print_callback);
  }

  if (cloud_print_connector_enabled_.GetPrefName().empty()) {
    cloud_print_connector_enabled_.Init(
        prefs::kCloudPrintProxyEnabled, profile->GetPrefs(),
        cloud_print_callback);
  }

  SetupCloudPrintConnectorSection();
  RefreshCloudPrintStatusFromService();
}

void LocalDiscoveryUIHandler::OnCloudPrintPrefsChanged() {
  SetupCloudPrintConnectorSection();
}

void LocalDiscoveryUIHandler::ShowCloudPrintSetupDialog(
    const base::ListValue* args) {
  base::RecordAction(base::UserMetricsAction("Options_EnableCloudPrintProxy"));
  // Open the connector enable page in the current tab.
  content::OpenURLParams params(cloud_devices::GetCloudPrintEnableURL(
                                    GetCloudPrintProxyService()->proxy_id()),
                                content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_ui()->GetWebContents()->OpenURL(params);
}

void LocalDiscoveryUIHandler::HandleDisableCloudPrintConnector(
    const base::ListValue* args) {
  base::RecordAction(base::UserMetricsAction("Options_DisableCloudPrintProxy"));
  GetCloudPrintProxyService()->DisableForUser();
}

void LocalDiscoveryUIHandler::SetupCloudPrintConnectorSection() {
  bool cloud_print_connector_allowed =
      !cloud_print_connector_enabled_.IsManaged() ||
      cloud_print_connector_enabled_.GetValue();
  base::Value allowed(cloud_print_connector_allowed);

  std::string email;
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail) &&
      cloud_print_connector_allowed) {
    email = profile->GetPrefs()->GetString(prefs::kCloudPrintEmail);
  }
  base::Value disabled(email.empty());

  base::string16 label_str;
  if (email.empty()) {
    label_str = l10n_util::GetStringFUTF16(
        IDS_LOCAL_DISCOVERY_CLOUD_PRINT_CONNECTOR_DISABLED_LABEL,
        l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT));
  } else {
    label_str = l10n_util::GetStringFUTF16(
        IDS_CLOUD_PRINT_CONNECTOR_ENABLED_LABEL,
        l10n_util::GetStringUTF16(IDS_GOOGLE_CLOUD_PRINT),
        base::UTF8ToUTF16(email));
  }
  base::Value label(label_str);

  web_ui()->CallJavascriptFunctionUnsafe(
      "local_discovery.setupCloudPrintConnectorSection", disabled, label,
      allowed);
}

void LocalDiscoveryUIHandler::RefreshCloudPrintStatusFromService() {
  auto* service = GetCloudPrintProxyService();
  if (service)
    service->RefreshStatusFromService();
}

CloudPrintProxyService* LocalDiscoveryUIHandler::GetCloudPrintProxyService() {
  return CloudPrintProxyServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));
}
#endif  // defined(CLOUD_PRINT_CONNECTOR_UI_AVAILABLE)

}  // namespace local_discovery
