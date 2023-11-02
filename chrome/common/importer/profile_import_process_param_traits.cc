// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/profile_import_process_param_traits.h"

// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#include "chrome/common/importer/profile_import_process_param_traits_macros.h"

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#undef CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_MACROS_H_
#include "chrome/common/importer/profile_import_process_param_traits_macros.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_MACROS_H_
#include "chrome/common/importer/profile_import_process_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_MACROS_H_
#include "chrome/common/importer/profile_import_process_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_MACROS_H_
#include "chrome/common/importer/profile_import_process_param_traits_macros.h"
}  // namespace IPC

#include "mojo/public/cpp/base/string16_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<chrome::mojom::ImportedPasswordFormDataView,
                  importer::ImportedPasswordForm>::
    Read(chrome::mojom::ImportedPasswordFormDataView data,
         importer::ImportedPasswordForm* out) {
  if (!data.ReadScheme(&out->scheme) ||
      !data.ReadSignonRealm(&out->signon_realm) || !data.ReadUrl(&out->url) ||
      !data.ReadAction(&out->action) ||
      !data.ReadUsernameElement(&out->username_element) ||
      !data.ReadUsernameValue(&out->username_value) ||
      !data.ReadPasswordElement(&out->password_element) ||
      !data.ReadPasswordValue(&out->password_value)) {
    return false;
  }

  out->blocked_by_user = data.blocked_by_user();
  return true;
}

}  // namespace mojo
