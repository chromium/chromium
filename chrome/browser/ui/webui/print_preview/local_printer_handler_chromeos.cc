// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/local_printer_ash.h"
#include "chrome/browser/ash/printing/ipp_client_info_calculator.h"
#include "chrome/browser/ash/printing/local_printer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/printing/printer_capabilities.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/printing_restrictions.h"
#include "printing/buildflags/buildflags.h"  // USE_CUPS
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings_conversion_chromeos.h"
#include "url/gurl.h"

namespace printing {

namespace {

bool IsManagedPrinter(const chromeos::Printer& printer) {
  return printer.source() == chromeos::Printer::SRC_POLICY;
}

bool IsSecureIppPrinter(const chromeos::Printer& printer) {
  return printer.GetProtocol() == chromeos::Printer::PrinterProtocol::kIpps ||
         printer.GetProtocol() == chromeos::Printer::PrinterProtocol::kIppUsb;
}

bool IsActiveUserAffiliated() {
  const user_manager::User* user =
      user_manager::UserManager::IsInitialized()
          ? user_manager::UserManager::Get()->GetActiveUser()
          : nullptr;
  return user ? user->IsAffiliated() : false;
}

void OnGetPrintersComplete(
    LocalPrinterHandlerChromeos::AddedPrintersCallback callback,
    std::vector<chromeos::Printer> printers) {
  if (!printers.empty()) {
    base::ListValue list;
    for (const chromeos::Printer& p : printers) {
      list.Append(LocalPrinterHandlerChromeos::PrinterToValue(p));
    }
    std::move(callback).Run(std::move(list));
  }
}

base::DictValue AddProfileUsernameToJobSettings(
    base::DictValue settings,
    const std::optional<std::string>& username) {
  if (username.has_value() && !username->empty()) {
    settings.Set(kSettingUsername, *username);
    settings.Set(kSettingSendUserInfo, true);
  }
  return settings;
}

base::DictValue AddOAuthTokenToJobSettings(
    base::DictValue settings,
    base::optional_ref<const std::string> oauth_token) {
  if (oauth_token.has_value() && !oauth_token->empty()) {
    settings.Set(kSettingChromeOSAccessOAuthToken, *oauth_token);
  }
  return settings;
}

base::DictValue AddIppClientInfoToJobSettings(
    base::DictValue settings,
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

template <typename MojoOptionType,
          typename MojoOptionValueType,
          typename ValueType>
base::DictValue SimpleTypePrintOptionMojomToDict(
    const mojo::StructPtr<MojoOptionType>& print_option,
    base::RepeatingCallback<ValueType(MojoOptionValueType option_value)>
        convert_func) {
  base::DictValue result;

  if (print_option->default_value) {
    result.Set(kManagedPrintOptions_DefaultValue,
               convert_func.Run(*print_option->default_value));
  }
  if (print_option->allowed_values) {
    base::ListValue allowed_values;
    for (const auto& allowed_value : *print_option->allowed_values) {
      allowed_values.Append(convert_func.Run(allowed_value));
    }
    result.Set(kManagedPrintOptions_AllowedValues, std::move(allowed_values));
  }

  return result;
}

template <typename MojoOptionType,
          typename MojoOptionValueType,
          typename ValueType>
base::DictValue CustomTypePrintOptionMojomToDict(
    const mojo::StructPtr<MojoOptionType>& print_option,
    base::RepeatingCallback<ValueType(MojoOptionValueType option_value)>
        convert_func) {
  base::DictValue result;

  if (print_option->default_value) {
    result.Set(kManagedPrintOptions_DefaultValue,
               convert_func.Run(*print_option->default_value));
  }
  if (print_option->allowed_values) {
    base::ListValue allowed_values;
    for (const auto& allowed_value : *print_option->allowed_values) {
      allowed_values.Append(convert_func.Run(*allowed_value));
    }
    result.Set(kManagedPrintOptions_AllowedValues, std::move(allowed_values));
  }

  return result;
}

template <typename T,
          typename Func,
          typename = std::enable_if_t<std::is_invocable_v<Func, const T&>>>
base::DictValue PrintOptionToValue(
    const chromeos::Printer::PrintOption<T>& print_option,
    Func convert_func) {
  base::DictValue result;

  if (print_option.default_value.has_value()) {
    result.Set(kManagedPrintOptions_DefaultValue,
               convert_func(*print_option.default_value));
  }
  base::ListValue allowed_values;
  for (const auto& allowed_value : print_option.allowed_values) {
    allowed_values.Append(convert_func(allowed_value));
  }
  result.Set(kManagedPrintOptions_AllowedValues, std::move(allowed_values));

  return result;
}

}  // namespace

// static
std::unique_ptr<LocalPrinterHandlerChromeos>
LocalPrinterHandlerChromeos::Create(
    content::WebContents* preview_web_contents) {
  auto handler =
      std::make_unique<LocalPrinterHandlerChromeos>(preview_web_contents);
#if BUILDFLAG(USE_CUPS)
  handler->local_printer_ = ash::LocalPrinter::Get();
#endif
  return handler;
}

std::unique_ptr<LocalPrinterHandlerChromeos>
LocalPrinterHandlerChromeos::CreateForTesting(
    ash::LocalPrinter* local_printer,
    std::unique_ptr<ash::printing::IppClientInfoCalculator>
        ipp_client_info_calculator) {
  auto handler = std::make_unique<LocalPrinterHandlerChromeos>(nullptr);
  handler->local_printer_ = local_printer;
  handler->ipp_client_info_calculator_ = std::move(ipp_client_info_calculator);
  return handler;
}

LocalPrinterHandlerChromeos::LocalPrinterHandlerChromeos(
    content::WebContents* preview_web_contents)
    : preview_web_contents_(preview_web_contents) {}

LocalPrinterHandlerChromeos::~LocalPrinterHandlerChromeos() = default;

// static
base::DictValue LocalPrinterHandlerChromeos::PrinterToValue(
    const chromeos::Printer& printer) {
  base::DictValue value;
  value.Set(kSettingDeviceName, printer.id());
  value.Set(kSettingPrinterName, printer.display_name());
  value.Set(kSettingPrinterDescription, printer.description());
  value.Set(kCUPSEnterprisePrinter,
            printer.source() == chromeos::Printer::SRC_POLICY);
  value.Set(kPrinterStatus, StatusToValue(printer.printer_status()));
  value.Set(kManagedPrintOptions,
            ManagedPrintOptionsToValue(printer.print_job_options()));
  return value;
}

// static
base::DictValue LocalPrinterHandlerChromeos::CapabilityToValue(
    base::optional_ref<const chromeos::Printer> printer,
    const std::optional<::printing::PrinterSemanticCapsAndDefaults>& caps) {
  if (!caps.has_value()) {
    return base::DictValue();
  }
  CHECK(printer.has_value());

  RecordDpi(*caps);

  // Non-const copy so AssemblePrinterSettings can take it.
  // AssemblePrinterSettings will modify caps_copy value.
  ::printing::PrinterSemanticCapsAndDefaults caps_copy(*caps);

  return AssemblePrinterSettings(
      printer->id(),
      PrinterBasicInfo(printer->id(), printer->display_name(),
                       printer->description(),
                       PrinterBasicInfoOptions{
                           {kCUPSEnterprisePrinter,
                            (printer->source() == chromeos::Printer::SRC_POLICY)
                                ? kValueTrue
                                : kValueFalse}}),
      printer->HasSecureProtocol(), &caps_copy);
}

// static
base::DictValue LocalPrinterHandlerChromeos::StatusToValue(
    const chromeos::CupsPrinterStatus& status) {
  base::DictValue dict;
  dict.Set("printerId", status.GetPrinterId());
  dict.Set("timestamp",
           status.GetTimestamp().InMillisecondsFSinceUnixEpochIgnoringNull());
  base::ListValue status_reasons;
  for (const chromeos::CupsPrinterStatus::CupsPrinterStatusReason& reason :
       status.GetStatusReasons()) {
    base::DictValue status_reason;
    status_reason.Set("reason", static_cast<int>(reason.GetReason()));
    status_reason.Set("severity", static_cast<int>(reason.GetSeverity()));
    status_reasons.Append(std::move(status_reason));
  }
  dict.Set("statusReasons", std::move(status_reasons));
  return dict;
}

// static
base::DictValue LocalPrinterHandlerChromeos::ManagedPrintOptionsToValue(
    const chromeos::Printer::ManagedPrintOptions& managed_print_options) {
  base::DictValue result;

  result.Set(kManagedPrintOptions_MediaSize,
             PrintOptionToValue(
                 managed_print_options.media_size,
                 [](const chromeos::Printer::Size& value) {
                   base::DictValue result;
                   result.Set(kManagedPrintOptions_SizeWidth, value.width);
                   result.Set(kManagedPrintOptions_SizeHeight, value.height);
                   return result;
                 }));

  result.Set(
      kManagedPrintOptions_MediaType,
      PrintOptionToValue(managed_print_options.media_type,
                         [](const std::string& value) { return value; }));

  result.Set(kManagedPrintOptions_Duplex,
             PrintOptionToValue(managed_print_options.duplex,
                                [](const chromeos::Printer::DuplexType& value) {
                                  return static_cast<int>(value);
                                }));

  result.Set(
      kManagedPrintOptions_Color,
      PrintOptionToValue(managed_print_options.color,
                         [](const bool& value) -> bool { return value; }));

  result.Set(
      kManagedPrintOptions_Dpi,
      PrintOptionToValue(
          managed_print_options.dpi, [](const chromeos::Printer::Dpi& value) {
            base::DictValue result;
            result.Set(kManagedPrintOptions_DpiHorizontal, value.horizontal);
            result.Set(kManagedPrintOptions_DpiVertical, value.vertical);
            return result;
          }));

  result.Set(
      kManagedPrintOptions_Quality,
      PrintOptionToValue(managed_print_options.quality,
                         [](const chromeos::Printer::QualityType& value) {
                           return static_cast<int>(value);
                         }));

  result.Set(
      kManagedPrintOptions_PrintAsImage,
      PrintOptionToValue(managed_print_options.print_as_image,
                         [](const bool& value) -> bool { return value; }));

  return result;
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
      // TODO(crbug.com/354842935): Replace by ash::AnnotatedAccountId.
      // TODO(crbug.com/479647640): Check if we should use current user than
      // primary user.
      session_manager::SessionManager::Get()->GetPrimarySession()->account_id(),
      base::BindOnce(OnGetPrintersComplete, std::move(callback))
          .Then(std::move(done_callback)));
}

void LocalPrinterHandlerChromeos::StartGetCapability(
    const std::string& device_name,
    GetCapabilityCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!local_printer_) {
    PRINTER_LOG(ERROR) << "Local printer not available (StartGetCapability)";
    std::move(callback).Run(base::DictValue());
    return;
  }
  local_printer_->GetCapability(
      // TODO(crbug.com/354842935): Replace by ash::AnnotatedAccountId.
      // TODO(crbug.com/479647640): Check if we should use current user than
      // primary user.
      session_manager::SessionManager::Get()->GetPrimarySession()->account_id(),
      device_name, base::BindOnce(CapabilityToValue).Then(std::move(callback)));
}

void LocalPrinterHandlerChromeos::StartPrint(
    const std::u16string& job_title,
    base::DictValue settings,
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
    base::DictValue settings) {
  GetAshJobSettings(std::move(printer_id), std::move(callback),
                    std::move(settings));
}

void LocalPrinterHandlerChromeos::GetAshJobSettings(
    std::string printer_id,
    AshJobSettingsCallback callback,
    base::DictValue settings) {
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
    base::DictValue settings) const {
  auto add_profile_username_callback =
      base::BindOnce(AddProfileUsernameToJobSettings, std::move(settings))
          .Then(std::move(callback));

  if (!session_manager::SessionManager::Get() ||
      !session_manager::SessionManager::Get()->GetActiveSession()) {
    LOG(ERROR) << "Session manager not initialized or no active session";
    std::move(add_profile_username_callback).Run(std::nullopt);
    return;
  }
  auto account =
      session_manager::SessionManager::Get()->GetPrimarySession()->account_id();
  auto* user = user_manager::UserManager::Get()->FindUser(account);
  CHECK(user);
  auto* profile_prefs = user->GetProfilePrefs();
  if (profile_prefs &&
      profile_prefs->GetBoolean(
          ash::prefs::kPrintingSendUsernameAndFilenameEnabled)) {
    std::move(add_profile_username_callback).Run(user->display_email());
  } else {
    std::move(add_profile_username_callback).Run(std::nullopt);
  }
}

void LocalPrinterHandlerChromeos::GetOAuthToken(
    const std::string& printer_id,
    AshJobSettingsCallback callback,
    base::DictValue settings) const {
  auto add_oauth_token_callback =
      base::BindOnce(AddOAuthTokenToJobSettings, std::move(settings))
          .Then(std::move(callback));

  local_printer_->GetOAuthAccessToken(
      // TODO(crbug.com/354842935): Replace by ash::AnnotatedAccountId.
      // TODO(crbug.com/479647640): Check if we should use current user than
      // primary user.
      session_manager::SessionManager::Get()->GetPrimarySession()->account_id(),
      printer_id, std::move(add_oauth_token_callback));
}

void LocalPrinterHandlerChromeos::GetIppClientInfo(
    const std::string& printer_id,
    AshJobSettingsCallback callback,
    base::DictValue settings) const {
  if (printer_id.empty()) {
    LOG(ERROR) << "Cannot call GetIppClientInfo: empty printer_id";
    std::move(callback).Run(std::move(settings));
    return;
  }

  std::optional<chromeos::Printer> printer = local_printer_->GetPrinter(
      session_manager::SessionManager::Get()->GetPrimarySession()->account_id(),
      printer_id);
  if (!printer) {
    LOG(ERROR) << "Cannot call GetIppClientInfo: printer not found";
    std::move(callback).Run(std::move(settings));
    return;
  }

  std::vector<mojom::IppClientInfoPtr> client_infos;
  auto os_info = GetIppClientInfoCalculator()->GetOsInfo();
  if (os_info) {
    client_infos.emplace_back(std::move(os_info));
  }
  if (IsManagedPrinter(*printer) && IsSecureIppPrinter(*printer) &&
      // TODO(crbug.com/354842935): Revisit to ensure if we should check
      // ActiveUser or PrimaryUser.
      IsActiveUserAffiliated()) {
    auto device_info = GetIppClientInfoCalculator()->GetDeviceInfo();
    if (device_info) {
      client_infos.push_back(std::move(device_info));
    }
  }

  std::move(callback).Run(AddIppClientInfoToJobSettings(
      std::move(settings), std::move(client_infos)));
}

void LocalPrinterHandlerChromeos::CallStartLocalPrint(
    scoped_refptr<base::RefCountedMemory> print_data,
    PrinterHandler::PrintCallback callback,
    base::DictValue settings) {
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
  local_printer_->GetEulaUrl(
      // TODO(crbug.com/354842935): Replace by ash::AnnotatedAccountId.
      // TODO(crbug.com/479647640): Check if we should use current user than
      // primary user.
      session_manager::SessionManager::Get()->GetPrimarySession()->account_id(),
      destination_id, base::BindOnce([](const GURL& url) {
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
      // TODO(crbug.com/354842935): Replace by ash::AnnotatedAccountId.
      // TODO(crbug.com/479647640): Check if we should use current user than
      // primary user.
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId(),
      printer_id, base::BindOnce([](const chromeos::CupsPrinterStatus& status) {
                    return StatusToValue(status);
                  }).Then(std::move(callback)));
}

ash::printing::IppClientInfoCalculator*
LocalPrinterHandlerChromeos::GetIppClientInfoCalculator() const {
  if (!ipp_client_info_calculator_) {
    ipp_client_info_calculator_ =
        ash::printing::IppClientInfoCalculator::Create();
  }
  return ipp_client_info_calculator_.get();
}

}  // namespace printing
