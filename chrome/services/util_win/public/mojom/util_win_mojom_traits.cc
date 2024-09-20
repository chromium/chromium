// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/services/util_win/public/mojom/util_win_mojom_traits.h"

#include <limits.h> /* UINT_MAX */

#include <utility>

#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/shortcut.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/wstring_mojom_traits.h"

namespace mojo {

// static
chrome::mojom::SelectFileDialogType EnumTraits<
    chrome::mojom::SelectFileDialogType,
    ui::SelectFileDialog::Type>::ToMojom(ui::SelectFileDialog::Type input) {
  switch (input) {
    case ui::SelectFileDialog::Type::SELECT_NONE:
      return chrome::mojom::SelectFileDialogType::kNone;
    case ui::SelectFileDialog::Type::SELECT_FOLDER:
      return chrome::mojom::SelectFileDialogType::kFolder;
    case ui::SelectFileDialog::Type::SELECT_UPLOAD_FOLDER:
      return chrome::mojom::SelectFileDialogType::kUploadFolder;
    case ui::SelectFileDialog::Type::SELECT_EXISTING_FOLDER:
      return chrome::mojom::SelectFileDialogType::kExistingFolder;
    case ui::SelectFileDialog::Type::SELECT_SAVEAS_FILE:
      return chrome::mojom::SelectFileDialogType::kSaveAsFile;
    case ui::SelectFileDialog::Type::SELECT_OPEN_FILE:
      return chrome::mojom::SelectFileDialogType::kOpenFile;
    case ui::SelectFileDialog::Type::SELECT_OPEN_MULTI_FILE:
      return chrome::mojom::SelectFileDialogType::kOpenMultiFile;
  }
  NOTREACHED_IN_MIGRATION();
  return chrome::mojom::SelectFileDialogType::kNone;
}

// static
bool EnumTraits<chrome::mojom::SelectFileDialogType,
                ui::SelectFileDialog::Type>::
    FromMojom(chrome::mojom::SelectFileDialogType input,
              ui::SelectFileDialog::Type* output) {
  switch (input) {
    case chrome::mojom::SelectFileDialogType::kNone:
      *output = ui::SelectFileDialog::Type::SELECT_NONE;
      return true;
    case chrome::mojom::SelectFileDialogType::kFolder:
      *output = ui::SelectFileDialog::Type::SELECT_FOLDER;
      return true;
    case chrome::mojom::SelectFileDialogType::kUploadFolder:
      *output = ui::SelectFileDialog::Type::SELECT_UPLOAD_FOLDER;
      return true;
    case chrome::mojom::SelectFileDialogType::kExistingFolder:
      *output = ui::SelectFileDialog::Type::SELECT_EXISTING_FOLDER;
      return true;
    case chrome::mojom::SelectFileDialogType::kSaveAsFile:
      *output = ui::SelectFileDialog::Type::SELECT_SAVEAS_FILE;
      return true;
    case chrome::mojom::SelectFileDialogType::kOpenFile:
      *output = ui::SelectFileDialog::Type::SELECT_OPEN_FILE;
      return true;
    case chrome::mojom::SelectFileDialogType::kOpenMultiFile:
      *output = ui::SelectFileDialog::Type::SELECT_OPEN_MULTI_FILE;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
chrome::mojom::CertificateType
EnumTraits<chrome::mojom::CertificateType, CertificateInfo::Type>::ToMojom(
    CertificateInfo::Type input) {
  switch (input) {
    case CertificateInfo::Type::NO_CERTIFICATE:
      return chrome::mojom::CertificateType::kNoCertificate;
    case CertificateInfo::Type::CERTIFICATE_IN_FILE:
      return chrome::mojom::CertificateType::kCertificateInFile;
    case CertificateInfo::Type::CERTIFICATE_IN_CATALOG:
      return chrome::mojom::CertificateType::kCertificateInCatalog;
  }
  NOTREACHED_IN_MIGRATION();
  return chrome::mojom::CertificateType::kNoCertificate;
}

// static
chrome::mojom::ShortcutOperation
EnumTraits<chrome::mojom::ShortcutOperation, ::base::win::ShortcutOperation>::
    ToMojom(::base::win::ShortcutOperation input) {
  switch (input) {
    case base::win::ShortcutOperation::kCreateAlways:
      return chrome::mojom::ShortcutOperation::kCreateAlways;
    case base::win::ShortcutOperation::kReplaceExisting:
      return chrome::mojom::ShortcutOperation::kReplaceExisting;
    case base::win::ShortcutOperation::kUpdateExisting:
      return chrome::mojom::ShortcutOperation::kUpdateExisting;
  }
  DUMP_WILL_BE_NOTREACHED();
  return chrome::mojom::ShortcutOperation::kCreateAlways;
}

// static
bool EnumTraits<chrome::mojom::ShortcutOperation,
                ::base::win::ShortcutOperation>::
    FromMojom(chrome::mojom::ShortcutOperation input,
              ::base::win::ShortcutOperation* output) {
  switch (input) {
    case chrome::mojom::ShortcutOperation::kCreateAlways:
      *output = base::win::ShortcutOperation::kCreateAlways;
      return true;
    case chrome::mojom::ShortcutOperation::kReplaceExisting:
      *output = base::win::ShortcutOperation::kReplaceExisting;
      return true;
    case chrome::mojom::ShortcutOperation::kUpdateExisting:
      *output = base::win::ShortcutOperation::kUpdateExisting;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
bool StructTraits<chrome::mojom::FileFilterSpecDataView, ui::FileFilterSpec>::
    Read(chrome::mojom::FileFilterSpecDataView input, ui::FileFilterSpec* out) {
  return input.ReadDescription(&out->description) &&
         input.ReadExtensionSpec(&out->extension_spec);
}

// static
bool EnumTraits<chrome::mojom::CertificateType, CertificateInfo::Type>::
    FromMojom(chrome::mojom::CertificateType input,
              CertificateInfo::Type* output) {
  switch (input) {
    case chrome::mojom::CertificateType::kNoCertificate:
      *output = CertificateInfo::Type::NO_CERTIFICATE;
      return true;
    case chrome::mojom::CertificateType::kCertificateInFile:
      *output = CertificateInfo::Type::CERTIFICATE_IN_FILE;
      return true;
    case chrome::mojom::CertificateType::kCertificateInCatalog:
      *output = CertificateInfo::Type::CERTIFICATE_IN_CATALOG;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
const std::u16string& StructTraits<
    chrome::mojom::InspectionResultDataView,
    ModuleInspectionResult>::location(const ModuleInspectionResult& input) {
  return input.location;
}
// static
const std::u16string& StructTraits<
    chrome::mojom::InspectionResultDataView,
    ModuleInspectionResult>::basename(const ModuleInspectionResult& input) {
  return input.basename;
}
// static
const std::u16string& StructTraits<
    chrome::mojom::InspectionResultDataView,
    ModuleInspectionResult>::product_name(const ModuleInspectionResult& input) {
  return input.product_name;
}
// static
const std::u16string& StructTraits<
    chrome::mojom::InspectionResultDataView,
    ModuleInspectionResult>::description(const ModuleInspectionResult& input) {
  return input.description;
}
// static
const std::u16string& StructTraits<
    chrome::mojom::InspectionResultDataView,
    ModuleInspectionResult>::version(const ModuleInspectionResult& input) {
  return input.version;
}
// static
chrome::mojom::CertificateType
StructTraits<chrome::mojom::InspectionResultDataView, ModuleInspectionResult>::
    certificate_type(const ModuleInspectionResult& input) {
  return EnumTraits<chrome::mojom::CertificateType,
                    CertificateInfo::Type>::ToMojom(input.certificate_info
                                                        .type);
}
// static
const base::FilePath&
StructTraits<chrome::mojom::InspectionResultDataView, ModuleInspectionResult>::
    certificate_path(const ModuleInspectionResult& input) {
  return input.certificate_info.path;
}
// static
const std::u16string&
StructTraits<chrome::mojom::InspectionResultDataView, ModuleInspectionResult>::
    certificate_subject(const ModuleInspectionResult& input) {
  return input.certificate_info.subject;
}

// static
bool StructTraits<
    chrome::mojom::InspectionResultDataView,
    ModuleInspectionResult>::Read(chrome::mojom::InspectionResultDataView input,
                                  ModuleInspectionResult* out) {
  return input.ReadLocation(&out->location) &&
         input.ReadBasename(&out->basename) &&
         input.ReadProductName(&out->product_name) &&
         input.ReadDescription(&out->description) &&
         input.ReadVersion(&out->version) &&
         input.ReadCertificateType(&out->certificate_info.type) &&
         input.ReadCertificatePath(&out->certificate_info.path) &&
         input.ReadCertificateSubject(&out->certificate_info.subject);
}

// static
base::span<const uint8_t> StructTraits<chrome::mojom::ClsIdDataView,
                                       ::CLSID>::bytes(const ::CLSID& input) {
  return base::make_span(reinterpret_cast<const uint8_t*>(&input),
                         sizeof(input));
}

// static
bool StructTraits<chrome::mojom::ClsIdDataView, ::CLSID>::Read(
    chrome::mojom::ClsIdDataView data,
    ::CLSID* out) {
  ArrayDataView<uint8_t> bytes_view;
  data.GetBytesDataView(&bytes_view);
  DCHECK_EQ(bytes_view.size(), sizeof(*out));

  const ::CLSID* cls_id = reinterpret_cast<const ::CLSID*>(bytes_view.data());

  memcpy(out, cls_id, sizeof(*out));
  return true;
}

//  static
bool StructTraits<chrome::mojom::ShortcutPropertiesDataView,
                  base::win::ShortcutProperties>::
    Read(chrome::mojom::ShortcutPropertiesDataView input,
         base::win::ShortcutProperties* out) {
  out->icon_index = input.icon_index();
  out->dual_mode = input.dual_mode();
  out->options = input.options();

  // out->toast_activator_clsid
  return input.ReadTarget(&out->target) &&
         input.ReadWorkingDir(&out->working_dir) &&
         input.ReadToastActivatorClsid(&out->toast_activator_clsid) &&
         input.ReadDescription(&out->description) &&
         input.ReadArguments(&out->arguments) && input.ReadIcon(&out->icon) &&
         input.ReadAppId(&out->app_id);
}

// static
bool StructTraits<chrome::mojom::AntiVirusProductDataView,
                  metrics::SystemProfileProto_AntiVirusProduct>::
    Read(chrome::mojom::AntiVirusProductDataView input,
         metrics::SystemProfileProto_AntiVirusProduct* output) {
  output->set_product_state(
      static_cast<metrics::SystemProfileProto_AntiVirusState>(input.state()));

  output->set_product_name_hash(input.product_name_hash());
  output->set_product_version_hash(input.product_version_hash());

  // Protobufs have the ability to distinguish unset strings from empty strings,
  // while mojo doesn't. To preserve current behavior, make sure empty product
  // name and versions are not set in the protobuf.
  std::string product_name;
  if (!input.ReadProductName(&product_name))
    return false;
  if (!product_name.empty())
    output->set_product_name(std::move(product_name));

  std::string product_version;
  if (!input.ReadProductVersion(&product_version))
    return false;
  if (!product_version.empty())
    output->set_product_version(std::move(product_version));

  return true;
}

// static
bool StructTraits<chrome::mojom::TpmIdentifierDataView,
                  metrics::SystemProfileProto_TpmIdentifier>::
    Read(chrome::mojom::TpmIdentifierDataView input,
         metrics::SystemProfileProto_TpmIdentifier* output) {
  if (input.manufacturer_id() == 0u) {
    // If manufacturer_id is it's default value metrics will not be
    // reported.
    return false;
  }
  output->set_manufacturer_id(input.manufacturer_id());

  std::optional<std::string> manufacturer_version;
  if (input.ReadManufacturerVersion(&manufacturer_version)) {
    if (manufacturer_version.has_value()) {
      output->set_manufacturer_version(std::move(manufacturer_version.value()));
    }
  }

  std::optional<std::string> manufacturer_version_info;
  if (input.ReadManufacturerVersionInfo(&manufacturer_version_info)) {
    if (manufacturer_version_info.has_value()) {
      output->set_manufacturer_version_info(
          std::move(manufacturer_version_info.value()));
    }
  }

  std::optional<std::string> tpm_specific_version;
  if (input.ReadTpmSpecificVersion(&tpm_specific_version)) {
    if (tpm_specific_version.has_value()) {
      output->set_tpm_specific_version(std::move(tpm_specific_version.value()));
    }
  }

  // If the hashes are 0, they have not been set and wont be reported
  if (input.manufacturer_version_hash() != 0u) {
    output->set_manufacturer_version_hash(input.manufacturer_version_hash());
  }

  if (input.manufacturer_version_info_hash() != 0u) {
    output->set_manufacturer_version_info_hash(
        input.manufacturer_version_info_hash());
  }

  if (input.tpm_specific_version_hash() != 0u) {
    output->set_tpm_specific_version_hash(input.tpm_specific_version_hash());
  }

  return true;
}

}  // namespace mojo
