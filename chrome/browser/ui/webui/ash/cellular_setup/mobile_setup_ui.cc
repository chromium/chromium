// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cellular_setup/mobile_setup_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/mobile/mobile_activator.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "url/gurl.h"

namespace ash::cellular_setup {

namespace {

// Host page JS API function names.
const char kJsGetDeviceInfo[] = "getDeviceInfo";

const char kJsDeviceStatusChangedCallback[] =
    "mobile.MobileSetup.deviceStateChanged";
const char kJsGetDeviceInfoCallback[] =
    "mobile.MobileSetupPortal.onGotDeviceInfo";
const char kJsConnectivityChangedCallback[] =
    "mobile.MobileSetupPortal.onConnectivityChanged";

// TODO(tbarzic): Localize these strings.
const char16_t kDefaultActivationError[] =
    u"$1 is unable to connect to $2 at this time. Please try again later.";
const char16_t kCellularDisabledError[] =
    u"Mobile network connections are not currently enabled on this device.";
const char16_t kNoCellularDeviceError[] =
    u"Mobile network modem is not present.";
const char16_t kNoCellularServiceError[] =
    u"$1 is unable to connect at this time due to insufficient coverage.";

bool ActivationErrorRequiresCarrier(MobileActivator::ActivationError error) {
  return error == MobileActivator::ActivationError::kActivationFailed;
}

std::u16string GetActivationErrorMessage(MobileActivator::ActivationError error,
                                         const std::string& carrier) {
  // If the activation error message requires the carrier name, and none was
  // provider, fallback to kNoCellularServiceError.
  if (carrier.empty() && ActivationErrorRequiresCarrier(error)) {
    CHECK(!ActivationErrorRequiresCarrier(
        MobileActivator::ActivationError::kNoCellularService));
    return GetActivationErrorMessage(
        MobileActivator::ActivationError::kNoCellularService, carrier);
  }

  switch (error) {
    case MobileActivator::ActivationError::kNone:
      return std::u16string();
    case MobileActivator::ActivationError::kActivationFailed: {
      return base::ReplaceStringPlaceholders(
          kDefaultActivationError,
          std::vector<std::u16string>(
              {ui::GetChromeOSDeviceName(), base::UTF8ToUTF16(carrier)}),
          nullptr);
    }
    case MobileActivator::ActivationError::kCellularDisabled:
      return kCellularDisabledError;
    case MobileActivator::ActivationError::kNoCellularDevice:
      return kNoCellularDeviceError;
    case MobileActivator::ActivationError::kNoCellularService:
      return base::ReplaceStringPlaceholders(
          kNoCellularServiceError, ui::GetChromeOSDeviceName(), nullptr);
  }
  NOTREACHED_IN_MIGRATION() << "Unexpected activation error";
  return GetActivationErrorMessage(
      MobileActivator::ActivationError::kActivationFailed, carrier);
}

void DataRequestFailed(const std::string& service_path,
                       content::URLDataSource::GotDataCallback callback) {
  NET_LOG(ERROR) << "Data Request Failed for Mobile Setup: "
                 << NetworkPathId(service_path);
  scoped_refptr<base::RefCountedBytes> html_bytes(new base::RefCountedBytes);
  std::move(callback).Run(html_bytes.get());
}

// Keys for the dictionary that is set to activation UI and that contains the
// cellular network information.
namespace keys {

// The current activation state:
constexpr char kActivationState[] = "state";
constexpr char kActivationErrorMessage[] = "error";

// The cellular service properties:
constexpr char kCellularActivationType[] = "activation_type";
constexpr char kCarrier[] = "carrier";
constexpr char kPaymentPortalUrl[] = "payment_url";
constexpr char kPaymentPortalPostData[] = "post_data";

// Cellular device properties:
constexpr char kMeid[] = "MEID";
constexpr char kImei[] = "IMEI";
constexpr char kMdn[] = "MDN";

}  // namespace keys

// Generates dictionary value with cellular service and device information that
// can be sent to the UI as "device info".
// The dictionary will not contain any activation flow state.
// NOTE: This handles null |network| and |device| for convenience - it will
// return an empty dictionary if either is not set.
base::Value GetCellularNetworkInfoValue(const NetworkState* network,
                                        const DeviceState* device) {
  base::Value::Dict info;
  if (!device || !network)
    return base::Value(std::move(info));

  DCHECK_EQ(network->device_path(), device->path());

  info.Set(keys::kMeid, base::Value(device->meid()));
  info.Set(keys::kImei, base::Value(device->imei()));
  info.Set(keys::kMdn, base::Value(device->mdn()));
  info.Set(keys::kCarrier, base::Value(device->operator_name()));
  info.Set(keys::kCellularActivationType,
           base::Value(network->activation_type()));
  info.Set(keys::kPaymentPortalUrl, base::Value(network->payment_url()));
  info.Set(keys::kPaymentPortalPostData,
           base::Value(network->payment_post_data()));

  return base::Value(std::move(info));
}

}  // namespace

class MobileSetupUIHTMLSource : public content::URLDataSource {
 public:
  MobileSetupUIHTMLSource();

  MobileSetupUIHTMLSource(const MobileSetupUIHTMLSource&) = delete;
  MobileSetupUIHTMLSource& operator=(const MobileSetupUIHTMLSource&) = delete;

  ~MobileSetupUIHTMLSource() override {}

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL&) override { return "text/html"; }
  bool ShouldAddContentSecurityPolicy() override { return false; }
  bool AllowCaching() override {
    // Should not be cached to reflect dynamically-generated contents that may
    // depend on current settings.
    return false;
  }

 private:
  base::WeakPtrFactory<MobileSetupUIHTMLSource> weak_ptr_factory_{this};
};

// The handler for Javascript messages related to the "register" view.
class MobileSetupHandler : public content::WebUIMessageHandler,
                           public MobileActivator::Observer,
                           public NetworkStateHandlerObserver {
 public:
  MobileSetupHandler();

  MobileSetupHandler(const MobileSetupHandler&) = delete;
  MobileSetupHandler& operator=(const MobileSetupHandler&) = delete;

  ~MobileSetupHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

  // MobileActivator::Observer.
  void OnActivationStateChanged(
      const NetworkState* network,
      MobileActivator::PlanActivationState new_state,
      MobileActivator::ActivationError error) override;

 private:
  enum Type {
    TYPE_UNDETERMINED,
    // The network is not yet activated, and the webui is in activation flow.
    TYPE_ACTIVATION,
    // The network is activated, the webui displays network portal.
    TYPE_PORTAL,
    // Same as TYPE_PORTAL, but the network technology is LTE. The webui is
    // additionally aware of network manager state and whether the portal can be
    // reached.
    TYPE_PORTAL_LTE
  };

  void Reset();

  // Handlers for JS WebUI messages.
  void HandleGetDeviceInfo(const base::Value::List& args);

  // NetworkStateHandlerObserver implementation.
  void NetworkConnectionStateChanged(const NetworkState* network) override;
  void DefaultNetworkChanged(const NetworkState* default_network) override;

  // Updates |lte_portal_reachable_| for lte network |network| and notifies
  // webui of the new state if the reachability changed or |force_notification|
  // is set.
  void UpdatePortalReachability(const NetworkState* network,
                                bool force_notification);

  // Type of the mobilesetup webui deduced from received messages.
  Type type_;
  // Whether the mobile setup has been started.
  bool active_;
  // Whether portal page for lte networks can be reached in current network
  // connection state. This value is reflected in portal webui for lte networks.
  // Initial value is true.
  bool lte_portal_reachable_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::WeakPtrFactory<MobileSetupHandler> weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

MobileSetupUIHTMLSource::MobileSetupUIHTMLSource() {}

std::string MobileSetupUIHTMLSource::GetSource() {
  return chrome::kChromeUIMobileSetupHost;
}

void MobileSetupUIHTMLSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  // Sanity checks that activation was requested for an appropriate network.
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(path);

  if (!network) {
    NET_LOG(ERROR) << "Network for mobile setup not found: " << path;
    DataRequestFailed(path, std::move(callback));
    return;
  }

  if (!network->Matches(NetworkTypePattern::Cellular())) {
    NET_LOG(ERROR) << "Mobile setup attempt for non cellular network: "
                   << NetworkId(network);
    DataRequestFailed(path, std::move(callback));
    return;
  }

  if (network->payment_url().empty() &&
      network->activation_state() != shill::kActivationStateActivated) {
    NET_LOG(ERROR) << "Mobile setup network in unexpected state: "
                   << NetworkId(network)
                   << " payment_url: " << network->payment_url()
                   << " activation_state: " << network->activation_state();
    DataRequestFailed(path, std::move(callback));
    return;
  }

  const DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(
          network->device_path());
  if (!device) {
    NET_LOG(ERROR) << "Network device for mobile setup not found: "
                   << network->device_path();
    DataRequestFailed(path, std::move(callback));
    return;
  }

  NET_LOG(EVENT) << "Starting mobile setup: " << NetworkId(network);
  base::Value::Dict strings;

  strings.Set("view_account_error_title",
              l10n_util::GetStringUTF16(IDS_MOBILE_VIEW_ACCOUNT_ERROR_TITLE));
  strings.Set("view_account_error_message",
              l10n_util::GetStringUTF16(IDS_MOBILE_VIEW_ACCOUNT_ERROR_MESSAGE));
  strings.Set("title", l10n_util::GetStringUTF16(IDS_MOBILE_SETUP_TITLE));
  strings.Set("close_button", l10n_util::GetStringUTF16(IDS_CLOSE));
  strings.Set("cancel_button", l10n_util::GetStringUTF16(IDS_CANCEL));
  strings.Set("ok_button", l10n_util::GetStringUTF16(IDS_OK));

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &strings);

  // mobile_setup_ui.cc will only be triggered from the detail page for
  // activated cellular network.
  DCHECK(network->activation_state() == shill::kActivationStateActivated);
  std::string html_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_MOBILE_SETUP_PORTAL_PAGE_HTML);
  std::string full_html = webui::GetI18nTemplateHtml(html_string, strings);
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(full_html)));
}

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupHandler
//
////////////////////////////////////////////////////////////////////////////////
MobileSetupHandler::MobileSetupHandler()
    : type_(TYPE_UNDETERMINED), active_(false), lte_portal_reachable_(true) {}

MobileSetupHandler::~MobileSetupHandler() {
  Reset();
}

void MobileSetupHandler::OnActivationStateChanged(
    const NetworkState* network,
    MobileActivator::PlanActivationState state,
    MobileActivator::ActivationError error) {
  DCHECK_EQ(TYPE_ACTIVATION, type_);
  if (!web_ui())
    return;

  NetworkStateHandler* network_state_handler =
      NetworkHandler::Get()->network_state_handler();
  const DeviceState* device =
      network ? network_state_handler->GetDeviceState(network->device_path())
              : nullptr;

  // First generate cellular properties dictionary, if cellular service and
  // device are available.
  base::Value info = GetCellularNetworkInfoValue(network, device);

  // Add the current activation flow state.
  info.GetDict().Set(keys::kActivationState, static_cast<int>(state));
  info.GetDict().Set(
      keys::kActivationErrorMessage,
      GetActivationErrorMessage(error, device ? device->operator_name() : ""));

  CallJavascriptFunction(kJsDeviceStatusChangedCallback, info);
}

void MobileSetupHandler::OnJavascriptDisallowed() {
  Reset();
}

void MobileSetupHandler::Reset() {
  if (!active_)
    return;
  active_ = false;

  if (type_ == TYPE_ACTIVATION) {
    MobileActivator::GetInstance()->RemoveObserver(this);
    MobileActivator::GetInstance()->TerminateActivation();
  } else if (type_ == TYPE_PORTAL_LTE) {
    network_state_handler_observer_.Reset();
  }
}

void MobileSetupHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kJsGetDeviceInfo,
      base::BindRepeating(&MobileSetupHandler::HandleGetDeviceInfo,
                          base::Unretained(this)));
}

void MobileSetupHandler::HandleGetDeviceInfo(const base::Value::List& args) {
  DCHECK_NE(TYPE_ACTIVATION, type_);
  if (!web_ui())
    return;

  std::string path = web_ui()->GetWebContents()->GetURL().path();
  if (path.empty())
    return;

  active_ = true;
  AllowJavascript();

  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  // The path has an extra '/' in the front. (e.g. It is '//service/5' instead
  // of '/service/5'.
  const NetworkState* network = nsh->GetNetworkState(path.substr(1));
  if (!network) {
    CallJavascriptFunction(kJsGetDeviceInfoCallback,
                           base::Value(base::Value::Type::DICT));
    return;
  }

  // If this is the initial call, update the network status and start observing
  // network changes, but only for LTE networks. The other networks should
  // ignore network status.
  if (type_ == TYPE_UNDETERMINED) {
    if (network->network_technology() == shill::kNetworkTechnologyLte ||
        network->network_technology() == shill::kNetworkTechnologyLteAdvanced) {
      type_ = TYPE_PORTAL_LTE;
      network_state_handler_observer_.Observe(nsh);
      // Update the network status and notify the webui. This is the initial
      // network state so the webui should be notified no matter what.
      UpdatePortalReachability(network, true /* force notification */);
    } else {
      type_ = TYPE_PORTAL;
      // For non-LTE networks network state is ignored, so report the portal is
      // reachable, so it gets shown.
      CallJavascriptFunction(kJsConnectivityChangedCallback, base::Value(true));
    }
  }

  const DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(
          network->device_path());
  CallJavascriptFunction(kJsGetDeviceInfoCallback,
                         GetCellularNetworkInfoValue(network, device));
}

void MobileSetupHandler::DefaultNetworkChanged(
    const NetworkState* default_network) {
  if (!web_ui())
    return;

  std::string path = web_ui()->GetWebContents()->GetURL().path().substr(1);
  if (path.empty())
    return;

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(path);
  if (!network) {
    NET_LOG(ERROR) << "Service for activation lost: " << path;
    web_ui()->GetWebContents()->Close();
    return;
  }

  UpdatePortalReachability(network, false /* do not force notification */);
}

void MobileSetupHandler::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (!web_ui())
    return;

  std::string path = web_ui()->GetWebContents()->GetURL().path().substr(1);
  if (path.empty() || path != network->path())
    return;

  UpdatePortalReachability(network, false /* do not force notification */);
}

void MobileSetupHandler::UpdatePortalReachability(const NetworkState* network,
                                                  bool force_notification) {
  DCHECK(web_ui());

  DCHECK_EQ(type_, TYPE_PORTAL_LTE);

  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  bool portal_reachable =
      (network->IsConnectedState() ||
       (nsh->DefaultNetwork() &&
        nsh->DefaultNetwork()->connection_state() == shill::kStateOnline));

  if (force_notification || portal_reachable != lte_portal_reachable_) {
    CallJavascriptFunction(kJsConnectivityChangedCallback,
                           base::Value(portal_reachable));
  }

  lte_portal_reachable_ = portal_reachable;
}

////////////////////////////////////////////////////////////////////////////////
//
// MobileSetupUI
//
////////////////////////////////////////////////////////////////////////////////

MobileSetupUI::MobileSetupUI(content::WebUI* web_ui) : ui::WebDialogUI(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<MobileSetupHandler>());

  // Set up the chrome://mobilesetup/ source.
  content::URLDataSource::Add(Profile::FromWebUI(web_ui),
                              std::make_unique<MobileSetupUIHTMLSource>());
}

MobileSetupUI::~MobileSetupUI() = default;

}  // namespace ash::cellular_setup
