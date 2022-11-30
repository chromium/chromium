// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/av_products.h"

#include <iwscapi.h>
#include <objbase.h>
#include <stddef.h>
#include <wbemidl.h>
#include <windows.h>
#include <wrl/client.h>
#include <wscapi.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/base_paths_win.h"
#include "base/file_version_info.h"
#include "base/file_version_info_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "components/variations/hashing.h"

namespace {

// This is an undocumented structure returned from querying the "productState"
// uint32 from the AntiVirusProduct in WMI.
// http://neophob.com/2010/03/wmi-query-windows-securitycenter2/ gives a good
// summary and testing was also done with a variety of AV products to determine
// these values as accurately as possible.
#pragma pack(push)
#pragma pack(1)
struct PRODUCT_STATE {
  uint8_t unknown_1 : 4;
  uint8_t definition_state : 4;  // 1 = Out of date, 0 = Up to date.
  uint8_t unknown_2 : 4;
  uint8_t security_state : 4;  //  0 = Inactive, 1 = Active, 2 = Snoozed.
  uint8_t security_provider;   // matches WSC_SECURITY_PROVIDER in wscapi.h.
  uint8_t unknown_3;
};
#pragma pack(pop)

// Filter any part of a product string that looks like it might be a version
// number. Returns true if the part should be removed from the product name.
bool ShouldFilterPart(const std::string& str) {
  // Special case for "360" (used by Norton), "365" (used by Kaspersky) and
  // "NOD32" (used by ESET).
  if (str == "365" || str == "360" || str == "NOD32")
    return false;
  for (const auto ch : str) {
    if (isdigit(ch))
      return true;
  }
  return false;
}

// Helper function for expanding all environment variables in |path|.
std::wstring ExpandEnvironmentVariables(const std::wstring& path) {
  static const DWORD kMaxBuffer = 32 * 1024;  // Max according to MSDN.
  std::wstring path_expanded;
  DWORD path_len = MAX_PATH;
  do {
    DWORD result = ExpandEnvironmentStrings(
        path.c_str(), base::WriteInto(&path_expanded, path_len), path_len);
    if (!result) {
      // Failed to expand variables. Return the original string.
      DPLOG(ERROR) << path;
      break;
    }
    if (result <= path_len)
      return path_expanded.substr(0, result - 1);
    path_len = result;
  } while (path_len < kMaxBuffer);

  return path;
}

// Helper function to take a |path| to a file, that might contain environment
// strings, and read the file version information in |product_version|. Returns
// true if it was possible to extract the file information correctly.
bool GetProductVersion(std::wstring* path, std::string* product_version) {
  base::FilePath full_path(ExpandEnvironmentVariables(*path));

#if !defined(_WIN64)
  if (!base::PathExists(full_path)) {
    // On 32-bit builds, path might contain C:\Program Files (x86) instead of
    // C:\Program Files.
    base::ReplaceFirstSubstringAfterOffset(path, 0, L"%ProgramFiles%",
                                           L"%ProgramW6432%");
    full_path = base::FilePath(ExpandEnvironmentVariables(*path));
  }
#endif  // !defined(_WIN64)
  std::unique_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(full_path));

  // It is not an error if the product version cannot be read, so continue in
  // this case.
  if (version_info.get()) {
    FileVersionInfoWin* version_info_win =
        static_cast<FileVersionInfoWin*>(version_info.get());
    std::string version_str =
        base::UTF16ToUTF8(version_info_win->product_version());

    *product_version = std::move(version_str);
    return true;
  }

  return false;
}

internal::ResultCode FillAntiVirusProductsFromWSC(
    bool report_full_names,
    std::vector<AvProduct>* products) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<AvProduct> result_list;

  Microsoft::WRL::ComPtr<IWSCProductList> product_list;
  HRESULT result =
      CoCreateInstance(__uuidof(WSCProductList), nullptr, CLSCTX_INPROC_SERVER,
                       IID_PPV_ARGS(&product_list));
  if (FAILED(result))
    return internal::ResultCode::kFailedToCreateInstance;

  result = product_list->Initialize(WSC_SECURITY_PROVIDER_ANTIVIRUS);
  if (FAILED(result))
    return internal::ResultCode::kFailedToInitializeProductList;

  LONG product_count;
  result = product_list->get_Count(&product_count);
  if (FAILED(result))
    return internal::ResultCode::kFailedToGetProductCount;

  for (LONG i = 0; i < product_count; i++) {
    Microsoft::WRL::ComPtr<IWscProduct> product;
    result = product_list->get_Item(i, &product);
    if (FAILED(result))
      return internal::ResultCode::kFailedToGetItem;

    static_assert(metrics::SystemProfileProto::AntiVirusState::
                          SystemProfileProto_AntiVirusState_STATE_ON ==
                      static_cast<metrics::SystemProfileProto::AntiVirusState>(
                          WSC_SECURITY_PRODUCT_STATE_ON),
                  "proto and API values must be the same");
    static_assert(metrics::SystemProfileProto::AntiVirusState::
                          SystemProfileProto_AntiVirusState_STATE_OFF ==
                      static_cast<metrics::SystemProfileProto::AntiVirusState>(
                          WSC_SECURITY_PRODUCT_STATE_OFF),
                  "proto and API values must be the same");
    static_assert(metrics::SystemProfileProto::AntiVirusState::
                          SystemProfileProto_AntiVirusState_STATE_SNOOZED ==
                      static_cast<metrics::SystemProfileProto::AntiVirusState>(
                          WSC_SECURITY_PRODUCT_STATE_SNOOZED),
                  "proto and API values must be the same");
    static_assert(metrics::SystemProfileProto::AntiVirusState::
                          SystemProfileProto_AntiVirusState_STATE_EXPIRED ==
                      static_cast<metrics::SystemProfileProto::AntiVirusState>(
                          WSC_SECURITY_PRODUCT_STATE_EXPIRED),
                  "proto and API values must be the same");

    AvProduct av_product;
    WSC_SECURITY_PRODUCT_STATE product_state;
    result = product->get_ProductState(&product_state);
    if (FAILED(result))
      return internal::ResultCode::kFailedToGetProductState;

    if (!metrics::SystemProfileProto_AntiVirusState_IsValid(product_state))
      return internal::ResultCode::kProductStateInvalid;

    av_product.set_product_state(
        static_cast<metrics::SystemProfileProto::AntiVirusState>(
            product_state));

    base::win::ScopedBstr product_name;
    result = product->get_ProductName(product_name.Receive());
    if (FAILED(result))
      return internal::ResultCode::kFailedToGetProductName;
    std::string name = internal::TrimVersionOfAvProductName(base::SysWideToUTF8(
        std::wstring(product_name.Get(), product_name.Length())));
    product_name.Release();
    if (report_full_names)
      av_product.set_product_name(name);
    av_product.set_product_name_hash(variations::HashName(name));

    base::win::ScopedBstr remediation_path;
    result = product->get_RemediationPath(remediation_path.Receive());
    if (FAILED(result))
      return internal::ResultCode::kFailedToGetRemediationPath;
    std::wstring path_str(remediation_path.Get(), remediation_path.Length());
    remediation_path.Release();

    std::string product_version;
    // Not a failure if the product version cannot be read from the file on
    // disk.
    if (GetProductVersion(&path_str, &product_version)) {
      if (report_full_names)
        av_product.set_product_version(product_version);
      av_product.set_product_version_hash(
          variations::HashName(product_version));
    }

    result_list.push_back(av_product);
  }

  *products = std::move(result_list);

  return internal::ResultCode::kSuccess;
}

internal::ResultCode FillAntiVirusProductsFromWMI(
    bool report_full_names,
    std::vector<AvProduct>* products) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<AvProduct> result_list;

  Microsoft::WRL::ComPtr<IWbemLocator> wmi_locator;
  HRESULT hr =
      ::CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&wmi_locator));
  if (FAILED(hr))
    return internal::ResultCode::kFailedToCreateInstance;

  Microsoft::WRL::ComPtr<IWbemServices> wmi_services;
  hr = wmi_locator->ConnectServer(
      base::win::ScopedBstr(L"ROOT\\SecurityCenter2").Get(), nullptr, nullptr,
      nullptr, 0, nullptr, nullptr, &wmi_services);
  if (FAILED(hr))
    return internal::ResultCode::kFailedToConnectToWMI;

  hr = ::CoSetProxyBlanket(wmi_services.Get(), RPC_C_AUTHN_WINNT,
                           RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                           RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
  if (FAILED(hr))
    return internal::ResultCode::kFailedToSetSecurityBlanket;

  // This interface is available on Windows Vista and above, and is officially
  // undocumented.
  base::win::ScopedBstr query_language(L"WQL");
  base::win::ScopedBstr query(L"SELECT * FROM AntiVirusProduct");
  Microsoft::WRL::ComPtr<IEnumWbemClassObject> enumerator;

  hr = wmi_services->ExecQuery(
      query_language.Get(), query.Get(),
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
      &enumerator);
  if (FAILED(hr))
    return internal::ResultCode::kFailedToExecWMIQuery;

  // Iterate over the results of the WMI query. Each result will be an
  // AntiVirusProduct instance.
  while (true) {
    Microsoft::WRL::ComPtr<IWbemClassObject> class_object;
    ULONG items_returned = 0;
    hr = enumerator->Next(WBEM_INFINITE, 1, &class_object, &items_returned);
    if (FAILED(hr))
      return internal::ResultCode::kFailedToIterateResults;

    if (hr == WBEM_S_FALSE || items_returned == 0)
      break;

    AvProduct av_product;
    av_product.set_product_state(
        metrics::SystemProfileProto::AntiVirusState::
            SystemProfileProto_AntiVirusState_STATE_ON);

    // See definition of PRODUCT_STATE structure above for how this is being
    // used.
    base::win::ScopedVariant product_state;
    hr = class_object->Get(L"productState", 0, product_state.Receive(), 0, 0);

    if (FAILED(hr) || product_state.type() != VT_I4)
      return internal::ResultCode::kFailedToGetProductState;

    LONG state_val = V_I4(product_state.ptr());
    PRODUCT_STATE product_state_struct;
    std::copy(reinterpret_cast<const char*>(&state_val),
              reinterpret_cast<const char*>(&state_val) + sizeof state_val,
              reinterpret_cast<char*>(&product_state_struct));
    // Map the values from product_state_struct to the proto values.
    switch (product_state_struct.security_state) {
      case 0:
        av_product.set_product_state(
            metrics::SystemProfileProto::AntiVirusState::
                SystemProfileProto_AntiVirusState_STATE_OFF);
        break;
      case 1:
        av_product.set_product_state(
            metrics::SystemProfileProto::AntiVirusState::
                SystemProfileProto_AntiVirusState_STATE_ON);
        break;
      case 2:
        av_product.set_product_state(
            metrics::SystemProfileProto::AntiVirusState::
                SystemProfileProto_AntiVirusState_STATE_SNOOZED);
        break;
      default:
        // unknown state.
        return internal::ResultCode::kProductStateInvalid;
    }

    base::win::ScopedVariant display_name;
    hr = class_object->Get(L"displayName", 0, display_name.Receive(), 0, 0);

    if (FAILED(hr) || display_name.type() != VT_BSTR)
      return internal::ResultCode::kFailedToGetProductName;

    // Owned by ScopedVariant.
    BSTR temp_bstr = V_BSTR(display_name.ptr());
    std::string name = internal::TrimVersionOfAvProductName(base::SysWideToUTF8(
        std::wstring(temp_bstr, ::SysStringLen(temp_bstr))));

    if (report_full_names)
      av_product.set_product_name(name);
    av_product.set_product_name_hash(variations::HashName(name));

    base::win::ScopedVariant exe_path;
    hr = class_object->Get(L"pathToSignedProductExe", 0, exe_path.Receive(), 0,
                           0);

    if (FAILED(hr) || exe_path.type() != VT_BSTR)
      return internal::ResultCode::kFailedToGetRemediationPath;

    temp_bstr = V_BSTR(exe_path.ptr());
    std::wstring path_str(temp_bstr, ::SysStringLen(temp_bstr));

    std::string product_version;
    // Not a failure if the product version cannot be read from the file on
    // disk.
    if (GetProductVersion(&path_str, &product_version)) {
      if (report_full_names)
        av_product.set_product_version(product_version);
      av_product.set_product_version_hash(
          variations::HashName(product_version));
    }

    result_list.push_back(av_product);
  }

  *products = std::move(result_list);

  return internal::ResultCode::kSuccess;
}

void MaybeAddTrusteerEndpointProtection(bool report_full_names,
                                        std::vector<AvProduct>* products) {
  // Trusteer Rapport does not register with WMI or Security Center so do some
  // "best efforts" detection here.

  // Rapport always installs into 32-bit Program Files in directory
  // %DIR_PROGRAM_FILESX86%\Trusteer\Rapport
  base::FilePath binary_path;
  if (!base::PathService::Get(base::DIR_PROGRAM_FILESX86, &binary_path))
    return;

  binary_path = binary_path.AppendASCII("Trusteer")
                    .AppendASCII("Rapport")
                    .AppendASCII("bin")
                    .AppendASCII("RapportService.exe");

  if (!base::PathExists(binary_path))
    return;

  std::wstring mutable_path_str(binary_path.value());
  std::string product_version;

  if (!GetProductVersion(&mutable_path_str, &product_version))
    return;

  AvProduct av_product;

  // Assume enabled, no easy way of knowing for sure.
  av_product.set_product_state(metrics::SystemProfileProto::AntiVirusState::
                                   SystemProfileProto_AntiVirusState_STATE_ON);

  // Taken from Add/Remove programs as the product name.
  std::string product_name("Trusteer Endpoint Protection");
  if (report_full_names) {
    av_product.set_product_name(product_name);
    av_product.set_product_version(product_version);
  }
  av_product.set_product_name_hash(variations::HashName(product_name));
  av_product.set_product_version_hash(variations::HashName(product_version));

  products->push_back(av_product);
}

void MaybeAddCarbonBlack(bool report_full_names,
                         std::vector<AvProduct>* products) {
  // Carbon Black does not register with WMI or Security Center so do some
  // "best efforts" detection here.

  // Look for driver in the Windows drivers directory.
  base::FilePath driver_path;
  if (!base::PathService::Get(base::DIR_SYSTEM, &driver_path))
    return;

  driver_path = driver_path.AppendASCII("drivers").AppendASCII("parity.sys");

  if (!base::PathExists(driver_path))
    return;

  std::wstring mutable_path_str(driver_path.value());
  std::string product_version;

  // Note: this is full version including patch level.
  if (!GetProductVersion(&mutable_path_str, &product_version))
    return;

  AvProduct av_product;

  // Assume enabled, no easy way of knowing for sure.
  av_product.set_product_state(metrics::SystemProfileProto::AntiVirusState::
                                   SystemProfileProto_AntiVirusState_STATE_ON);

  // This name is taken from the driver properties.
  std::string product_name("CB Protection");
  if (report_full_names) {
    av_product.set_product_name(product_name);
    av_product.set_product_version(product_version);
  }
  av_product.set_product_name_hash(variations::HashName(product_name));
  av_product.set_product_version_hash(variations::HashName(product_version));

  products->push_back(av_product);
}

void MaybeAddTopaz(bool report_full_names, std::vector<AvProduct>* products) {
  // Topaz does not register with WMI or Security Center so do some "best
  // efforts" detection here.

  base::FilePath binary_path;
  // Always installed in C:\Program Files, never (x86). Force this from 32-bit
  // on 64-bit installs by using DIR_PROGRAM_FILES6432.
  if (!base::PathService::Get(base::DIR_PROGRAM_FILES6432, &binary_path))
    return;

  // Old versions are under the 'Diebold' directory.
  const base::FilePath::StringPieceType kPossibleInstallPaths[] = {
      FILE_PATH_LITERAL("Topaz OFD"), FILE_PATH_LITERAL("Diebold")};

  for (const auto install_path : kPossibleInstallPaths) {
    // Topaz ships with both 32-bit and 64-bit components. Check for just one
    // here.
    auto dll_path = binary_path.Append(install_path)
                        .Append(FILE_PATH_LITERAL("Warsaw"))
                        .Append(FILE_PATH_LITERAL("wslbdhm32.dll"));
    if (!base::PathExists(dll_path))
      continue;

    std::wstring mutable_path_str(dll_path.value());
    std::string product_version;

    // Note: this is full version including patch level.
    if (!GetProductVersion(&mutable_path_str, &product_version))
      continue;

    AvProduct av_product;

    // Assume enabled, no easy way of knowing for sure.
    av_product.set_product_state(
        metrics::SystemProfileProto::AntiVirusState::
            SystemProfileProto_AntiVirusState_STATE_ON);

    // This name is taken from the properties of the installed files.
    std::string product_name("Topaz OFD");
    if (report_full_names) {
      av_product.set_product_name(product_name);
      av_product.set_product_version(product_version);
    }
    av_product.set_product_name_hash(variations::HashName(product_name));
    av_product.set_product_version_hash(variations::HashName(product_version));

    products->push_back(av_product);

    break;
  }
}

void MaybeAddUnregisteredAntiVirusProducts(bool report_full_names,
                                           std::vector<AvProduct>* products) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  MaybeAddTrusteerEndpointProtection(report_full_names, products);
  MaybeAddCarbonBlack(report_full_names, products);
  MaybeAddTopaz(report_full_names, products);
}

}  // namespace

std::vector<AvProduct> GetAntiVirusProducts(bool report_full_names) {
  base::win::AssertComApartmentType(base::win::ComApartmentType::STA);

  internal::ResultCode result = internal::ResultCode::kGenericFailure;

  std::vector<AvProduct> av_products;

  base::win::OSInfo* os_info = base::win::OSInfo::GetInstance();

  // Windows Security Center APIs are not available on Server products.
  // See https://msdn.microsoft.com/en-us/library/bb432506.aspx.
  if (os_info->version_type() == base::win::SUITE_SERVER) {
    result = internal::ResultCode::kWSCNotAvailable;
  } else {
    // The WSC interface is preferred here as it's fully documented, but only
    // available on Windows 8 and above, so instead use the undocumented WMI
    // interface on Windows 7 and below.
    if (os_info->version() >= base::win::Version::WIN8)
      result = FillAntiVirusProductsFromWSC(report_full_names, &av_products);
    else
      result = FillAntiVirusProductsFromWMI(report_full_names, &av_products);
  }

  MaybeAddUnregisteredAntiVirusProducts(report_full_names, &av_products);

  base::UmaHistogramEnumeration("UMA.AntiVirusMetricsProvider.Result", result);

  return av_products;
}

namespace internal {

std::string TrimVersionOfAvProductName(const std::string& av_product) {
  auto av_product_parts = base::SplitString(
      av_product, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (av_product_parts.size() >= 2) {
    // Skipping first element, remove any that look like version numbers.
    av_product_parts.erase(
        std::remove_if(av_product_parts.begin() + 1, av_product_parts.end(),
                       ShouldFilterPart),
        av_product_parts.end());
  }

  return base::JoinString(av_product_parts, " ");
}

}  // namespace internal
