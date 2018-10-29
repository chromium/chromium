// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/public/mojom/shell_util_mojom_traits.h"

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
bool StructTraits<chrome::mojom::FileFilterSpecDataView, ui::FileFilterSpec>::
    Read(chrome::mojom::FileFilterSpecDataView input, ui::FileFilterSpec* out) {
  return input.ReadDescription(&out->description) &&
         input.ReadExtensionSpec(&out->extension_spec);
}

}  // namespace mojo
