// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/motherboard.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/wmi.h"
#endif

namespace metrics {
namespace {

struct MotherboardDetails {
  std::optional<std::string> manufacturer;
  std::optional<std::string> model;
  std::optional<std::string> bios_manufacturer;
  std::optional<std::string> bios_version;
  std::optional<Motherboard::BiosType> bios_type;
};

#if BUILDFLAG(IS_LINUX)
using base::FilePath;
using base::PathExists;
using base::ReadFileToString;
using base::TrimWhitespaceASCII;
using base::TRIM_TRAILING;

MotherboardDetails ReadMotherboardDetails() {
  constexpr FilePath::CharType kDmiPath[] = "/sys/devices/virtual/dmi/id";
  constexpr FilePath::CharType kEfiPath[] = "/sys/firmware/efi";
  const FilePath dmi_path(kDmiPath);
  MotherboardDetails details;
  std::string temp;
  if (ReadFileToString(dmi_path.Append("board_vendor"), &temp)) {
    details.manufacturer =
        std::string(TrimWhitespaceASCII(temp, TRIM_TRAILING));
  }
  if (ReadFileToString(dmi_path.Append("board_name"), &temp)) {
    details.model = std::string(TrimWhitespaceASCII(temp, TRIM_TRAILING));
  }
  if (ReadFileToString(dmi_path.Append("bios_vendor"), &temp)) {
    details.bios_manufacturer =
        std::string(TrimWhitespaceASCII(temp, TRIM_TRAILING));
  }
  if (ReadFileToString(dmi_path.Append("bios_version"), &temp)) {
    details.bios_version =
        std::string(TrimWhitespaceASCII(temp, TRIM_TRAILING));
  }
  if (PathExists(FilePath(kEfiPath))) {
    details.bios_type = Motherboard::BiosType::kUefi;
  } else {
    details.bios_type = Motherboard::BiosType::kLegacy;
  }
  return details;
}
#endif

#if BUILDFLAG(IS_WIN)
using Microsoft::WRL::ComPtr;
using base::win::ScopedBstr;
using base::win::ScopedVariant;

std::optional<std::string> ReadStringMember(
    ComPtr<IWbemClassObject> class_object,
    const wchar_t* key) {
  ScopedVariant variant;
  HRESULT hr = class_object->Get(key, 0, variant.Receive(), 0, 0);
  if (SUCCEEDED(hr) && variant.type() == VT_BSTR) {
    const auto len = ::SysStringLen(V_BSTR(variant.ptr()));
    std::wstring wstr(V_BSTR(variant.ptr()), len);
    return base::WideToUTF8(wstr);
  }
  return {};
}

void ReadWin32BaseBoard(const ComPtr<IWbemServices>& services,
                        std::optional<std::string>* manufacturer,
                        std::optional<std::string>* model) {
  static constexpr wchar_t kManufacturer[] = L"Manufacturer";
  static constexpr wchar_t kProduct[] = L"Product";
  static constexpr wchar_t kQueryProcessor[] =
      L"SELECT Manufacturer,Product FROM Win32_BaseBoard";

  ComPtr<IEnumWbemClassObject> enumerator_base_board;
  HRESULT hr = services->ExecQuery(
      ScopedBstr(L"WQL").Get(), ScopedBstr(kQueryProcessor).Get(),
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
      &enumerator_base_board);
  if (FAILED(hr) || !enumerator_base_board.Get())
    return;

  ComPtr<IWbemClassObject> class_object;
  ULONG items_returned = 0;
  hr = enumerator_base_board->Next(WBEM_INFINITE, 1, &class_object,
                                   &items_returned);
  if (FAILED(hr) || !items_returned)
    return;
  *manufacturer = ReadStringMember(class_object, kManufacturer);
  *model = ReadStringMember(class_object, kProduct);
}

void ReadWin32Bios(const ComPtr<IWbemServices>& services,
                   std::optional<std::string>* bios_manufacturer,
                   std::optional<std::string>* bios_version) {
  static constexpr wchar_t kManufacturer[] = L"Manufacturer";
  static constexpr wchar_t kVersion[] = L"Version";
  static constexpr wchar_t kQueryProcessor[] =
      L"SELECT Manufacturer,Version FROM Win32_BIOS";

  ComPtr<IEnumWbemClassObject> enumerator_base_board;
  HRESULT hr = services->ExecQuery(
      ScopedBstr(L"WQL").Get(), ScopedBstr(kQueryProcessor).Get(),
      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
      &enumerator_base_board);
  if (FAILED(hr) || !enumerator_base_board.Get())
    return;

  ComPtr<IWbemClassObject> class_object;
  ULONG items_returned = 0;
  hr = enumerator_base_board->Next(WBEM_INFINITE, 1, &class_object,
                                   &items_returned);
  if (FAILED(hr) || !items_returned)
    return;
  *bios_manufacturer = ReadStringMember(class_object, kManufacturer);
  *bios_version = ReadStringMember(class_object, kVersion);
}

void ReadFirmwareType(std::optional<Motherboard::BiosType>* bios_type) {
  FIRMWARE_TYPE firmware_type = FirmwareTypeUnknown;
  if (::GetFirmwareType(&firmware_type)) {
    if (firmware_type == FirmwareTypeBios) {
      *bios_type = Motherboard::BiosType::kLegacy;
    } else if (firmware_type == FirmwareTypeUefi) {
      *bios_type = Motherboard::BiosType::kUefi;
    } else {
      *bios_type = std::nullopt;
    }
  }
}

MotherboardDetails ReadMotherboardDetails() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  ComPtr<IWbemServices> services;
  MotherboardDetails details;
  if (!base::win::CreateLocalWmiConnection(true, &services))
    return details;
  ReadWin32BaseBoard(services, &details.manufacturer, &details.model);
  ReadWin32Bios(services, &details.bios_manufacturer, &details.bios_version);
  ReadFirmwareType(&details.bios_type);
  return details;
}
#endif
}  // namespace

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
Motherboard::Motherboard() {
  auto details = ReadMotherboardDetails();
  manufacturer_ = std::move(details.manufacturer),
  model_ = std::move(details.model),
  bios_manufacturer_ = std::move(details.bios_manufacturer),
  bios_version_ = std::move(details.bios_version),
  bios_type_ = std::move(details.bios_type);
}
#else
Motherboard::Motherboard() = default;
#endif

Motherboard::~Motherboard() = default;

}  // namespace metrics
