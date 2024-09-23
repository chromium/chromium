// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings_conversion_chromeos.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace printing {

namespace {

void OnGetPrintersComplete(
    LocalPrinterHandlerChromeos::AddedPrintersCallback callback,
    std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers) {
  if (!printers.empty()) {
    base::Value::List list;
    for (const crosapi::mojom::LocalDestinationInfoPtr& p : printers) {
      list.Append(LocalPrinterHandlerChromeos::PrinterToValue(*p));
    }
    std::move(callback).Run(std::move(list));
  }
}

base::Value::Dict AddProfileUsernameToJobSettings(
    base::Value::Dict settings,
    const std::optional<std::string>& username) {
  if (username.has_value() && !username->empty()) {
    settings.Set(kSettingUsername, *username);
    settings.Set(kSettingSendUserInfo, true);
  }
  return settings;
}

base::Value::Dict AddOAuthTokenToJobSettings(
    base::Value::Dict settings,
    crosapi::mojom::GetOAuthAccessTokenResultPtr oauth_result) {
  if (oauth_result->is_token()) {
    settings.Set(kSettingChromeOSAccessOAuthToken,
                 oauth_result->get_token()->token);
  } else if (oauth_result->is_error()) {
    LOG(ERROR) << "Error when obtaining an oauth token for a local printer";
  }
  return settings;
}

base::Value::Dict AddIppClientInfoToJobSettings(
    base::Value::Dict settings,
    std::vector<mojom::IppClientInfoPtr> client_infos) {
  std::vector<printing::mojom::IppClientInfo> client_info_list;
  client_info_list.reserve(client_infos.size());
  for (const printing::mojom::IppClientInfoPtr& client_info : client_infos) {
    client_info_list.emplace_back(std::move(*client_info));
  }
  if (!client_info_list.empty()) {
    settings.Set(kSettingIppClientInfo,
                 ConvertClientInfoToJobSetting(client_info_list));
  }
  return settings;
}

// Combines the 16 bit DPI values into a single 32 bit int. Places the DPI width
// value into bits 31-16 and the DPI height value into bits 15-0.
int HashDpiValue(int width, int height) {
  CHECK(width <= std::numeric_limits<uint16_t>::max() &&
        height <= std::numeric_limits<uint16_t>::max());
  return (width << 16) + height;
}

void RecordDpi(const PrinterSemanticCapsAndDefaults& capabilities) {
  const int default_width = capabilities.default_dpi.width();
  const int default_height = capabilities.default_dpi.height();
  if (default_width <= std::numeric_limits<uint16_t>::max() &&
      default_height <= std::numeric_limits<uint16_t>::max()) {
    base::UmaHistogramSparse("Printing.CUPS.DPI.Default",
                             HashDpiValue(default_width, default_height));
  }

  const std::vector<gfx::Size> dpis = capabilities.dpis;
  const int dpis_count = dpis.size();
  base::UmaHistogramCounts100("Printing.CUPS.DPI.Count", dpis_count);
  if (dpis_count == 0) {
    return;
  }

  std::optional<std::pair<int, int>> max_dpi;
  std::optional<std::pair<int, int>> min_dpi;
  for (const auto& dpi : dpis) {
    const int width = dpi.width();
    const int height = dpi.height();
    if (width <= std::numeric_limits<uint16_t>::max() &&
        height <= std::numeric_limits<uint16_t>::max()) {
      base::UmaHistogramSparse("Printing.CUPS.DPI.AllValues",
                               HashDpiValue(width, height));

      const int dpi_total = width * height;
      if (!min_dpi || dpi_total < (min_dpi->first * min_dpi->second)) {
        min_dpi = std::pair<int, int>(width, height);
      }
      if (!max_dpi || dpi_total > (max_dpi->first * max_dpi->second)) {
        max_dpi = std::pair<int, int>(width, height);
      }
    }
  }

  if (min_dpi) {
    base::UmaHistogramSparse("Printing.CUPS.DPI.Min",
                             HashDpiValue(min_dpi->first, min_dpi->second));
  }
  if (max_dpi) {
    base::UmaHistogramSparse("Printing.CUPS.DPI.Max",
                             HashDpiValue(max_dpi->first, max_dpi->second));
  }
}

}  // namespace

// static
std::unique_ptr<LocalPrinterHandlerChromeos>
LocalPrinterHandlerChromeos::Create(
    content::WebContents* preview_web_contents) {
  auto handler =
      std::make_unique<LocalPrinterHandlerChromeos>(preview_web_contents);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(crosapi::CrosapiManager::IsInitialized());
  handler->local_printer_ =
      crosapi::CrosapiManager::Get()->crosapi_ash()->local_printer_ash();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::LocalPrinter>()) {
    PRINTER_LOG(ERROR) << "Local printer not available (Create)";
    return handler;
  }
  handler->local_printer_ =
      service->GetRemote<crosapi::mojom::LocalPrinter>().get();
  handler->local_printer_version_ =
      service->GetInterfaceVersion<crosapi::mojom::LocalPrinter>();
#endif
  return handler;
}

std::unique_ptr<LocalPrinterHandlerChromeos>
LocalPrinterHandlerChromeos::CreateForTesting(
    crosapi::mojom::LocalPrinter* local_printer) {
  auto handler = std::make_unique<LocalPrinterHandlerChromeos>(nullptr);
  handler->local_printer_ = local_printer;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  handler->local_printer_version_ = INT_MAX;
#endif
  return handler;
}

LocalPrinterHandlerChromeos::LocalPrinterHandlerChromeos(
    content::WebContents* preview_web_contents)
    : preview_web_contents_(preview_web_contents) {}

LocalPrinterHandlerChromeos::~LocalPrinterHandlerChromeos() = default;

// static
base::Value::Dict LocalPrinterHandlerChromeos::PrinterToValue(
    const crosapi::mojom::LocalDestinationInfo& printer) {
  base::Value::Dict value;
  value.Set(kSettingDeviceName, printer.id);
  value.Set(kSettingPrinterName, printer.name);
  value.Set(kSettingPrinterDescription, printer.description);
  value.Set(kCUPSEnterprisePrinter, printer.configured_via_policy);
  value.Set(kPrinterStatus, printer.printer_status
                                ? StatusToValue(*printer.printer_status)
                                : base::Value::Dict());
  return value;
}

// static
base::Value::Dict LocalPrinterHandlerChromeos::CapabilityToValue(
    crosapi::mojom::CapabilitiesResponsePtr caps) {
  if (!caps) {
    return base::Value::Dict();
  }

  if (caps->capabilities) {
    RecordDpi(caps->capabilities.value());
  }

  return AssemblePrinterSettings(
      caps->basic_info->id,
      PrinterBasicInfo(
          caps->basic_info->id, caps->basic_info->name,
          caps->basic_info->description, 0, false,
          PrinterBasicInfoOptions{
              {kCUPSEnterprisePrinter, caps->basic_info->configured_via_policy
                                           ? kValueTrue
                                           : kValueFalse}}),
      caps->has_secure_protocol, base::OptionalToPtr(caps->capabilities));
}

// static
base::Value::Dict LocalPrinterHandlerChromeos::StatusToValue(
    const crosapi::mojom::PrinterStatus& status) {
  base::Value::Dict dict;
  dict.Set("printerId", status.printer_id);
  dict.Set("timestamp",
           status.timestamp.InMillisecondsFSinceUnixEpochIgnoringNull());
  base::Value::List status_reasons;
  for (const crosapi::mojom::StatusReasonPtr& reason_ptr :
       status.status_reasons) {
    base::Value::Dict status_reason;
    status_reason.Set("reason", static_cast<int>(reason_ptr->reason));
    status_reason.Set("severity", static_cast<int>(reason_ptr->severity));
    status_reasons.Append(std::move(status_reason));
  }
  dict.Set("statusReasons", std::move(status_reasons));
  return dict;
}

void LocalPrinterHandlerChromeos::Reset() {}

void LocalPrinterHandlerChromeos::GetDefaultPrinter(
    DefaultPrinterCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(b/172229329): Add default printers to ChromeOS.
  std::move(callback).Run(std::string());
}

void LocalPrinterHandlerChromeos::StartGetPrinters(
    AddedPrintersCallback callback,
    GetPrintersDoneCallback done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!local_printer_) {
    PRINTER_LOG(ERROR) << "Local printer not available (StartGetPrinters)";
    std::move(done_callback).Run();
    return;
  }
  local_printer_->GetPrinters(
      base::BindOnce(OnGetPrintersComplete, std::move(callback))
          .Then(std::move(done_callback)));
}

void LocalPrinterHandlerChromeos::StartGetCapability(
    const std::string& device_name,
    GetCapabilityCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!local_printer_) {
    PRINTER_LOG(ERROR) << "Local printer not available (StartGetCapability)";
    std::move(callback).Run(base::Value::Dict());
    return;
  }
  local_printer_->GetCapability(
      device_name, base::BindOnce(CapabilityToValue).Then(std::move(callback)));
}

void LocalPrinterHandlerChromeos::StartPrint(
    const std::u16string& job_title,
    base::Value::Dict settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    PrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  size_t size_in_kb = print_data->size() / 1024;
  base::UmaHistogramMemoryKB("Printing.CUPS.PrintDocumentSize", size_in_kb);

  std::string printer_id = *settings.FindString(kSettingDeviceName);
  auto call_start_local_print_callback =
      base::BindOnce(&LocalPrinterHandlerChromeos::CallStartLocalPrint,
                     weak_ptr_factory_.GetWeakPtr(), std::move(print_data),
                     std::move(callback));
  GetAshJobSettings(std::move(printer_id),
                    std::move(call_start_local_print_callback),
                    std::move(settings));
}

void LocalPrinterHandlerChromeos::GetAshJobSettingsForTesting(
    std::string printer_id,
    AshJobSettingsCallback callback,
    base::Value::Dict settings) {
  GetAshJobSettings(std::move(printer_id), std::move(callback),
                    std::move(settings));
}

void LocalPrinterHandlerChromeos::GetAshJobSettings(
    std::string printer_id,
    AshJobSettingsCallback callback,
    base::Value::Dict settings) {
  if (!local_printer_) {
    LOG(ERROR) << "Local printer not available";
    std::move(callback).Run(std::move(settings));
    return;
  }

  // Start a chain of async calls, `GetUsernamePerPolicy()` -> `GetOAuthToken()`
  // -> `GetIppClientInfo()` -> `callback()`.
  auto get_client_info_callback = base::BindOnce(
      &LocalPrinterHandlerChromeos::GetIppClientInfo,
      weak_ptr_factory_.GetWeakPtr(), printer_id, std::move(callback));
  auto get_oauth_token_callback =
      base::BindOnce(&LocalPrinterHandlerChromeos::GetOAuthToken,
                     weak_ptr_factory_.GetWeakPtr(), printer_id,
                     std::move(get_client_info_callback));
  GetUsernamePerPolicy(std::move(get_oauth_token_callback),
                       std::move(settings));
}

void LocalPrinterHandlerChromeos::GetUsernamePerPolicy(
    AshJobSettingsCallback callback,
    base::Value::Dict settings) const {
  auto add_profile_username_callback =
      base::BindOnce(AddProfileUsernameToJobSettings, std::move(settings))
          .Then(std::move(callback));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (local_printer_version_ <
      int{crosapi::mojom::LocalPrinter::MethodMinVersions::
              kGetUsernamePerPolicyMinVersion}) {
    LOG(WARNING) << "Ash LocalPrinter version " << local_printer_version_
                 << " does not support GetUsernamePerPolicy().";
    std::move(add_profile_username_callback).Run(std::nullopt);
    return;
  }
#endif

  local_printer_->GetUsernamePerPolicy(
      std::move(add_profile_username_callback));
}

void LocalPrinterHandlerChromeos::GetOAuthToken(
    const std::string& printer_id,
    AshJobSettingsCallback callback,
    base::Value::Dict settings) const {
  auto add_oauth_token_callback =
      base::BindOnce(AddOAuthTokenToJobSettings, std::move(settings))
          .Then(std::move(callback));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (local_printer_version_ <
      int{crosapi::mojom::LocalPrinter::MethodMinVersions::
              kGetOAuthAccessTokenMinVersion}) {
    LOG(WARNING) << "Ash LocalPrinter version " << local_printer_version_
                 << " does not support GetOAuthToken().";
    std::move(add_oauth_token_callback)
        .Run(crosapi::mojom::GetOAuthAccessTokenResult::NewNone(
            crosapi::mojom::OAuthNotNeeded::New()));
    return;
  }
#endif

  local_printer_->GetOAuthAccessToken(printer_id,
                                      std::move(add_oauth_token_callback));
}

void LocalPrinterHandlerChromeos::GetIppClientInfo(
    const std::string& printer_id,
    AshJobSettingsCallback callback,
    base::Value::Dict settings) const {
  auto add_ipp_client_info_callback =
      base::BindOnce(AddIppClientInfoToJobSettings, std::move(settings))
          .Then(std::move(callback));

  if (printer_id.empty()) {
    LOG(ERROR) << "Cannot call GetIppClientInfo: empty printer_id";
    std::move(add_ipp_client_info_callback).Run({});
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (local_printer_version_ <
      int{crosapi::mojom::LocalPrinter::MethodMinVersions::
              kGetIppClientInfoMinVersion}) {
    LOG(WARNING) << "Ash LocalPrinter version " << local_printer_version_
                 << " does not support GetIppClientInfo().";
    std::move(add_ipp_client_info_callback).Run({});
    return;
  }
#endif

  local_printer_->GetIppClientInfo(printer_id,
                                   std::move(add_ipp_client_info_callback));
}

void LocalPrinterHandlerChromeos::CallStartLocalPrint(
    scoped_refptr<base::RefCountedMemory> print_data,
    PrinterHandler::PrintCallback callback,
    base::Value::Dict settings) {
  StartLocalPrint(std::move(settings), std::move(print_data),
                  preview_web_contents_, std::move(callback));
}

void LocalPrinterHandlerChromeos::StartGetEulaUrl(
    const std::string& destination_id,
    GetEulaUrlCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!local_printer_) {
    PRINTER_LOG(ERROR) << "Local printer not available (StartGetEulaUrl)";
    std::move(callback).Run("");
    return;
  }
  local_printer_->GetEulaUrl(destination_id,
                             base::BindOnce([](const GURL& url) {
                               return url.spec();
                             }).Then(std::move(callback)));
}

void LocalPrinterHandlerChromeos::StartPrinterStatusRequest(
    const std::string& printer_id,
    PrinterStatusRequestCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!local_printer_) {
    PRINTER_LOG(ERROR)
        << "Local printer not available (StartPrinterStatusRequest)";
    std::move(callback).Run(std::nullopt);
    return;
  }
  local_printer_->GetStatus(
      printer_id, base::BindOnce([](crosapi::mojom::PrinterStatusPtr ptr) {
                    return StatusToValue(*ptr);
                  }).Then(std::move(callback)));
}

}  // namespace printing
