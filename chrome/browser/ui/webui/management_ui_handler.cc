// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui_handler.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/policy/status_uploader.h"
#include "chrome/browser/chromeos/policy/system_log_uploader.h"
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

const char kManagementLogUploadEnabled[] = "managementLogUploadEnabled";
const char kManagementReportActivityTimes[] = "managementReportActivityTimes";
const char kManagementReportHardwareStatus[] = "managementReportHardwareStatus";
const char kManagementReportNetworkInterfaces[] =
    "managementReportNetworkInterfaces";
const char kManagementReportUsers[] = "managementReportUsers";

namespace {

#if defined(OS_CHROMEOS)
base::string16 GetEnterpriseDisplayDomain(
    policy::BrowserPolicyConnectorChromeOS* connector) {
  if (!connector->IsEnterpriseManaged())
    return l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_NOT_MANAGED);

  std::string display_domain = connector->GetEnterpriseDisplayDomain();

  if (display_domain.empty()) {
    if (!connector->IsActiveDirectoryManaged())
      return l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_MANAGED);

    display_domain = connector->GetRealm();
  }

  return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_DEVICE_MANAGED_BY,
                                    base::UTF8ToUTF16(display_domain));
}

void AddChromeOSReportingInfo(base::Value* report_sources) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  // Only check for report status in managed environment.
  if (!connector->IsEnterpriseManaged())
    return;

  policy::DeviceCloudPolicyManagerChromeOS* manager =
      connector->GetDeviceCloudPolicyManager();

  if (!manager)
    return;

  if (manager->GetSystemLogUploader()->upload_enabled()) {
    report_sources->GetList().push_back(
        base::Value(kManagementLogUploadEnabled));
  }

  const policy::DeviceStatusCollector* collector =
      manager->GetStatusUploader()->device_status_collector();

  if (collector->report_activity_times()) {
    report_sources->GetList().push_back(
        base::Value(kManagementReportActivityTimes));
  }
  if (collector->report_hardware_status()) {
    report_sources->GetList().push_back(
        base::Value(kManagementReportHardwareStatus));
  }
  if (collector->report_network_interfaces()) {
    report_sources->GetList().push_back(
        base::Value(kManagementReportNetworkInterfaces));
  }
  if (collector->report_users()) {
    report_sources->GetList().push_back(base::Value(kManagementReportUsers));
  }
}
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)

std::vector<base::Value> GetPermissionsForExtension(
    scoped_refptr<const extensions::Extension> extension) {
  std::vector<base::Value> permission_messages;
  // Only consider force installed extensions
  if (!extensions::Manifest::IsPolicyLocation(extension->location()))
    return permission_messages;

  extensions::PermissionIDSet permissions =
      extensions::PermissionMessageProvider::Get()->GetAllPermissionIDs(
          extension->permissions_data()->active_permissions(),
          extension->GetType());

  const extensions::PermissionMessages messages =
      extensions::PermissionMessageProvider::Get()
          ->GetPowerfulPermissionMessages(permissions);

  for (const auto& message : messages)
    permission_messages.push_back(base::Value(message.message()));

  return permission_messages;
}

base::Value GetPowerfulExtensions(const extensions::ExtensionSet& extensions) {
  base::Value powerful_extensions(base::Value::Type::LIST);

  for (const auto& extension : extensions) {
    std::vector<base::Value> permission_messages =
        GetPermissionsForExtension(extension);

    // Only show extension on page if there is at least one permission
    // message to show.
    if (!permission_messages.empty()) {
      base::Value extension_to_add(base::Value::Type::DICTIONARY);
      extension_to_add.SetKey("name", base::Value(extension->name()));
      extension_to_add.SetKey("permissions",
                              base::Value(std::move(permission_messages)));
      powerful_extensions.GetList().push_back(std::move(extension_to_add));
    }
  }

  return powerful_extensions;
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

ManagementUIHandler::ManagementUIHandler() {}

ManagementUIHandler::~ManagementUIHandler() {}

void ManagementUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDeviceManagementStatus",
      base::BindRepeating(&ManagementUIHandler::HandleGetDeviceManagementStatus,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetReportingInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExtensions",
      base::BindRepeating(&ManagementUIHandler::HandleGetExtensions,
                          base::Unretained(this)));
}

void ManagementUIHandler::HandleGetDeviceManagementStatus(
    const base::ListValue* args) {
  AllowJavascript();
  base::RecordAction(base::UserMetricsAction("ManagementPageViewed"));

#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  base::Value managed_string(GetEnterpriseDisplayDomain(connector));
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            managed_string);
#else
  RejectJavascriptCallback(
      args->GetList()[0] /* callback_id */,
      base::Value("No device management status on Chrome desktop"));
#endif  // defined(OS_CHROMEOS)
}

void ManagementUIHandler::HandleGetReportingInfo(const base::ListValue* args) {
  base::Value report_sources(base::Value::Type::LIST);

// Only Chrome OS devices report status.
#if defined(OS_CHROMEOS)
  AddChromeOSReportingInfo(&report_sources);
#endif  // defined(OS_CHROMEOS)

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            report_sources);
}

void ManagementUIHandler::HandleGetExtensions(const base::ListValue* args) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // List of all enabled extensions
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
          ->enabled_extensions();

  base::Value powerful_extensions = GetPowerfulExtensions(extensions);

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            powerful_extensions);
#else
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            base::Value(base::Value::Type::LIST));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}
