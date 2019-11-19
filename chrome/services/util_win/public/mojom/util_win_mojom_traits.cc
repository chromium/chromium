// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/public/mojom/util_win_mojom_traits.h"

#include <utility>

#include "mojo/public/cpp/base/file_path_mojom_traits.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"

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
  NOTREACHED();
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
  NOTREACHED();
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
  NOTREACHED();
  return chrome::mojom::CertificateType::kNoCertificate;
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

  NOTREACHED();
  return false;
}

// static
const base::string16& StructTraits<
    chrome::mojom::InspectionResultDataView,
    ModuleInspectionResult>::location(const ModuleInspectionResult& input) {
  return input.location;
}
// static
const base::string16& StructTraits<
    chrome::mojom::InspectionResultDataView,
    ModuleInspectionResult>::basename(const ModuleInspectionResult& input) {
  return input.basename;
}
// static
const base::string16& StructTraits<
    chrome::mojom::InspectionResultDataView,
    ModuleInspectionResult>::product_name(const ModuleInspectionResult& input) {
  return input.product_name;
}
// static
const base::string16& StructTraits<
    chrome::mojom::InspectionResultDataView,
    ModuleInspectionResult>::description(const ModuleInspectionResult& input) {
  return input.description;
}
// static
const base::string16& StructTraits<
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
const base::string16&
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
bool StructTraits<chrome::mojom::FileFilterSpecDataView, ui::FileFilterSpec>::
    Read(chrome::mojom::FileFilterSpecDataView input, ui::FileFilterSpec* out) {
  return input.ReadDescription(&out->description) &&
         input.ReadExtensionSpec(&out->extension_spec);
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

}  // namespace mojo
