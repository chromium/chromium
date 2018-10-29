// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_UTIL_WIN_PUBLIC_MOJOM_SHELL_UTIL_MOJOM_TRAITS_H_
#define CHROME_SERVICES_UTIL_WIN_PUBLIC_MOJOM_SHELL_UTIL_MOJOM_TRAITS_H_

#include "base/strings/string16.h"
#include "chrome/services/util_win/public/mojom/shell_util_win.mojom.h"
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
struct StructTraits<chrome::mojom::FileFilterSpecDataView, ui::FileFilterSpec> {
  static const base::string16& description(const ui::FileFilterSpec& input) {
    return input.description;
  }
  static const base::string16& extension_spec(const ui::FileFilterSpec& input) {
    return input.extension_spec;
  }

  static bool Read(chrome::mojom::FileFilterSpecDataView data,
                   ui::FileFilterSpec* output);
};

}  // namespace mojo

#endif  // CHROME_SERVICES_UTIL_WIN_PUBLIC_MOJOM_SHELL_UTIL_MOJOM_TRAITS_H_
