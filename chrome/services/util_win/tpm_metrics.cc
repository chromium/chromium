// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/tpm_metrics.h"

#include <windows.h>

#include <comdef.h>

#include <set>

#include "base/metrics/histogram_functions.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_util_win.h"
#include "base/strings/sys_string_conversions.h"
#include "components/variations/hashing.h"

using TpmIdentifier = metrics::SystemProfileProto_TpmIdentifier;

// Functions imported from the Windows TpmCoreProvisioning.dll file.
int __stdcall TpmGet_ManufacturerId(unsigned int*);
typedef decltype(TpmGet_ManufacturerId)* pTpmGet_ManufacturerId;

// For the following functions, the first parameter is the size of the string
// while the second is the out-parameter containing a wchar_t*, where the size
// elements will be written to. If the second parameter is a nullptr, the
// function will instead return the size of the string.
int __stdcall TpmGet_ManufacturerVersion(unsigned int*, wchar_t*);
typedef decltype(TpmGet_ManufacturerVersion)* pTpmGet_ManufacturerVersion;

int __stdcall TpmGet_ManufacturerVersionInfo(unsigned int*, wchar_t*);
typedef decltype(TpmGet_ManufacturerVersionInfo)*
    pTpmGet_ManufacturerVersionInfo;

int __stdcall TpmGet_SpecVersion(unsigned int*, wchar_t*);
typedef decltype(TpmGet_SpecVersion)* pTpmGet_SpecVersion;

std::optional<TpmIdentifier> GetTpmIdentifier(bool report_full_names) {
  std::optional<TpmIdentifier> tpm_identifier;
  TpmIdentifier tpm_identifier_value;

  base::ScopedNativeLibrary hMod(base::ScopedNativeLibrary(
      base::LoadSystemLibrary(L"TpmCoreProvisioning.dll")));
  if (!hMod.is_valid()) {
    return tpm_identifier;
  }
  pTpmGet_ManufacturerId manufacturer_id =
      reinterpret_cast<pTpmGet_ManufacturerId>(
          hMod.GetFunctionPointer("TpmGet_ManufacturerId"));
  unsigned int id = 0;

  bool manufacturer_id_read_success =
      (manufacturer_id && manufacturer_id(&id) == 0);
  base::UmaHistogramBoolean("UMA.TPMMetricsProvider.ReadSuccess",
                            manufacturer_id_read_success);
  if (!manufacturer_id_read_success) {
    return tpm_identifier;
  }

  // These id's map to approved vendors as reported by the trusted computing
  // group. If the TPM id is not listed here, it will be marked as such and
  // no other data on the TPM will be gathered.
  const std::set<unsigned int> approved_tpm_ids = {
      1095582720, 1095652352, 1096043852, 1129530191, 1380926275, 1179408723,
      1196379975, 1213221120, 1112687437, 1213220096, 1212765001, 1229081856,
      1229346816, 1229870147, 1279610368, 1297303124, 1314082080, 1314150912,
      1314145024, 1363365709, 1397576526, 1397641984, 1397576515, 1398033696,
      1415073280, 1464156928};
  const bool approved_tpm_id = approved_tpm_ids.contains(id);
  base::UmaHistogramBoolean("UMA.TPMMetricsProvider.UnknownID",
                            !approved_tpm_id);
  // If the TPM ID is not in the list of approved ID detailed
  // metrics are not reported.
  if (!approved_tpm_id) {
    return tpm_identifier;
  }

  pTpmGet_ManufacturerVersion manufacturer_version =
      reinterpret_cast<pTpmGet_ManufacturerVersion>(
          hMod.GetFunctionPointer("TpmGet_ManufacturerVersion"));
  unsigned int size = 0;
  std::wstring manufacturer_version_wide;
  if (manufacturer_version(&size, nullptr) == 0) {
    manufacturer_version(&size,
                         base::WriteInto(&manufacturer_version_wide, size));
  }

  pTpmGet_ManufacturerVersion manufacturer_version_info =
      reinterpret_cast<pTpmGet_ManufacturerVersionInfo>(
          hMod.GetFunctionPointer("TpmGet_ManufacturerVersionInfo"));
  size = 0;
  std::wstring manufacturer_version_info_wide;
  if (manufacturer_version_info(&size, nullptr) == 0) {
    manufacturer_version_info(
        &size, base::WriteInto(&manufacturer_version_info_wide, size));
  }

  pTpmGet_SpecVersion tpm_spec_version = reinterpret_cast<pTpmGet_SpecVersion>(
      hMod.GetFunctionPointer("TpmGet_SpecVersion"));
  size = 0;
  std::wstring tpm_spec_version_wide;
  if (tpm_spec_version(&size, nullptr) == 0) {
    tpm_spec_version(&size, base::WriteInto(&tpm_spec_version_wide, size));
  }

  std::string manufacturer_version_string =
      base::SysWideToUTF8(manufacturer_version_wide);
  std::string manufacturer_version_info_string =
      base::SysWideToUTF8(manufacturer_version_info_wide);
  std::string tpm_spec_version_string =
      base::SysWideToUTF8(tpm_spec_version_wide);

  tpm_identifier_value.set_manufacturer_id(id);
  if (report_full_names) {
    if (!manufacturer_version_string.empty()) {
      tpm_identifier_value.set_manufacturer_version(
          manufacturer_version_string);
    }
    if (!manufacturer_version_info_string.empty()) {
      tpm_identifier_value.set_manufacturer_version_info(
          manufacturer_version_info_string);
    }
    if (!tpm_spec_version_string.empty()) {
      tpm_identifier_value.set_tpm_specific_version(tpm_spec_version_string);
    }
  }

  if (!manufacturer_version_string.empty()) {
    std::size_t manufacturer_version_hash =
        variations::HashName(manufacturer_version_string);
    tpm_identifier_value.set_manufacturer_version_hash(
        manufacturer_version_hash);
  }
  if (!manufacturer_version_info_string.empty()) {
    std::size_t manufacturer_version_info_hash =
        variations::HashName(manufacturer_version_info_string);
    tpm_identifier_value.set_manufacturer_version_info_hash(
        manufacturer_version_info_hash);
  }
  if (!tpm_spec_version_string.empty()) {
    std::size_t tpm_spec_version_hash =
        variations::HashName(tpm_spec_version_string);
    tpm_identifier_value.set_tpm_specific_version_hash(tpm_spec_version_hash);
  }

  tpm_identifier = tpm_identifier_value;
  return tpm_identifier;
}
