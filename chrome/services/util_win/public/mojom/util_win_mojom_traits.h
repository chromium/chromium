// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_PUBLIC_MOJOM_UTIL_WIN_MOJOM_TRAITS_H_
#define CHROME_SERVICES_UTIL_WIN_PUBLIC_MOJOM_UTIL_WIN_MOJOM_TRAITS_H_

#include <string>

#include "base/files/file_path.h"
#include "base/win/shortcut.h"
#include "chrome/browser/win/conflicts/module_info.h"
#include "chrome/browser/win/conflicts/module_info_util.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom-shared.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "ui/shell_dialogs/execute_select_file_win.h"

namespace mojo {

template <>
struct EnumTraits<chrome::mojom::SelectFileDialogType,
                  ui::SelectFileDialog::Type> {
  static chrome::mojom::SelectFileDialogType ToMojom(
      ui::SelectFileDialog::Type input);
  static bool FromMojom(chrome::mojom::SelectFileDialogType input,
                        ui::SelectFileDialog::Type* output);
};

template <>
struct EnumTraits<chrome::mojom::CertificateType, CertificateInfo::Type> {
  static chrome::mojom::CertificateType ToMojom(CertificateInfo::Type input);
  static bool FromMojom(chrome::mojom::CertificateType input,
                        CertificateInfo::Type* output);
};

template <>
struct EnumTraits<chrome::mojom::ShortcutOperation,
                  base::win::ShortcutOperation> {
  static chrome::mojom::ShortcutOperation ToMojom(
      base::win::ShortcutOperation input);
  static bool FromMojom(chrome::mojom::ShortcutOperation input,
                        base::win::ShortcutOperation* output);
};

template <>
struct StructTraits<chrome::mojom::FileFilterSpecDataView, ui::FileFilterSpec> {
  static const std::u16string& description(const ui::FileFilterSpec& input) {
    return input.description;
  }
  static const std::u16string& extension_spec(const ui::FileFilterSpec& input) {
    return input.extension_spec;
  }

  static bool Read(chrome::mojom::FileFilterSpecDataView data,
                   ui::FileFilterSpec* output);
};

template <>
struct StructTraits<chrome::mojom::ClsIdDataView, ::CLSID> {
  static base::span<const uint8_t> bytes(const ::CLSID& input);
  static bool Read(chrome::mojom::ClsIdDataView data, ::CLSID* out);
};

template <>
struct StructTraits<chrome::mojom::ShortcutPropertiesDataView,
                    base::win::ShortcutProperties> {
  static const base::FilePath& target(
      const base::win::ShortcutProperties& input) {
    return input.target;
  }
  static const base::FilePath& working_dir(
      const base::win::ShortcutProperties& input) {
    return input.working_dir;
  }
  static const std::wstring& arguments(
      const base::win::ShortcutProperties& input) {
    return input.arguments;
  }
  static const std::wstring& description(
      const base::win::ShortcutProperties& input) {
    return input.description;
  }
  static const base::FilePath& icon(
      const base::win::ShortcutProperties& input) {
    return input.icon;
  }
  static int icon_index(const base::win::ShortcutProperties& input) {
    return input.icon_index;
  }
  static const std::wstring& app_id(
      const base::win::ShortcutProperties& input) {
    return input.app_id;
  }
  static bool dual_mode(const base::win::ShortcutProperties& input) {
    return input.dual_mode;
  }
  static const CLSID& toast_activator_clsid(
      const base::win::ShortcutProperties& input) {
    return input.toast_activator_clsid;
  }
  static uint32_t options(const base::win::ShortcutProperties& input) {
    return input.options;
  }
  static bool Read(chrome::mojom::ShortcutPropertiesDataView data,
                   base::win::ShortcutProperties* output);
};

template <>
struct StructTraits<chrome::mojom::InspectionResultDataView,
                    ModuleInspectionResult> {
  static const std::u16string& location(const ModuleInspectionResult& input);
  static const std::u16string& basename(const ModuleInspectionResult& input);
  static const std::u16string& product_name(
      const ModuleInspectionResult& input);
  static const std::u16string& description(const ModuleInspectionResult& input);
  static const std::u16string& version(const ModuleInspectionResult& input);
  static chrome::mojom::CertificateType certificate_type(
      const ModuleInspectionResult& input);
  static const base::FilePath& certificate_path(
      const ModuleInspectionResult& input);
  static const std::u16string& certificate_subject(
      const ModuleInspectionResult& input);

  static bool Read(chrome::mojom::InspectionResultDataView data,
                   ModuleInspectionResult* output);
};

template <>
struct StructTraits<chrome::mojom::AntiVirusProductDataView,
                    metrics::SystemProfileProto_AntiVirusProduct> {
  static const std::string& product_name(
      const metrics::SystemProfileProto_AntiVirusProduct& input) {
    return input.product_name();
  }
  static uint32_t product_name_hash(
      const metrics::SystemProfileProto_AntiVirusProduct& input) {
    return input.product_name_hash();
  }
  static const std::string& product_version(
      const metrics::SystemProfileProto_AntiVirusProduct& input) {
    return input.product_version();
  }
  static uint32_t product_version_hash(
      const metrics::SystemProfileProto_AntiVirusProduct& input) {
    return input.product_version_hash();
  }
  static chrome::mojom::AntiVirusProductState state(
      const metrics::SystemProfileProto_AntiVirusProduct& input) {
    switch (input.product_state()) {
      case metrics::SystemProfileProto_AntiVirusState_STATE_ON:
        return chrome::mojom::AntiVirusProductState::kOn;
      case metrics::SystemProfileProto_AntiVirusState_STATE_OFF:
        return chrome::mojom::AntiVirusProductState::kOff;
      case metrics::SystemProfileProto_AntiVirusState_STATE_SNOOZED:
        return chrome::mojom::AntiVirusProductState::kSnoozed;
      case metrics::SystemProfileProto_AntiVirusState_STATE_EXPIRED:
        return chrome::mojom::AntiVirusProductState::kExpired;
    }
    NOTREACHED_IN_MIGRATION();
    return chrome::mojom::AntiVirusProductState::kOff;
  }

  static bool Read(chrome::mojom::AntiVirusProductDataView data,
                   metrics::SystemProfileProto_AntiVirusProduct* output);
};

template <>
struct StructTraits<chrome::mojom::TpmIdentifierDataView,
                    metrics::SystemProfileProto_TpmIdentifier> {
  static uint32_t manufacturer_id(
      const metrics::SystemProfileProto_TpmIdentifier& input) {
    return input.manufacturer_id();
  }
  static const std::string& manufacturer_version(
      const metrics::SystemProfileProto_TpmIdentifier& input) {
    return input.manufacturer_version();
  }
  static const std::string& manufacturer_version_info(
      const metrics::SystemProfileProto_TpmIdentifier& input) {
    return input.manufacturer_version_info();
  }
  static const std::string& tpm_specific_version(
      const metrics::SystemProfileProto_TpmIdentifier& input) {
    return input.tpm_specific_version();
  }

  static uint32_t manufacturer_version_hash(
      const metrics::SystemProfileProto_TpmIdentifier& input) {
    return input.manufacturer_version_hash();
  }

  static uint32_t manufacturer_version_info_hash(
      const metrics::SystemProfileProto_TpmIdentifier& input) {
    return input.manufacturer_version_info_hash();
  }

  static uint32_t tpm_specific_version_hash(
      const metrics::SystemProfileProto_TpmIdentifier& input) {
    return input.tpm_specific_version_hash();
  }

  static bool Read(chrome::mojom::TpmIdentifierDataView data,
                   metrics::SystemProfileProto_TpmIdentifier* output);
};

}  // namespace mojo

#endif  // CHROME_SERVICES_UTIL_WIN_PUBLIC_MOJOM_UTIL_WIN_MOJOM_TRAITS_H_
