// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PATCH_PUBLIC_MOJOM_FILE_PATCHER_MOJOM_TRAITS_H_
#define COMPONENTS_SERVICES_PATCH_PUBLIC_MOJOM_FILE_PATCHER_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "components/services/patch/public/mojom/file_patcher.mojom-shared.h"
#include "components/zucchini/zucchini.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

namespace mojo {

template <>
struct EnumTraits<patch::mojom::ZucchiniStatus, zucchini::status::Code> {
  using enum zucchini::status::Code;

  static patch::mojom::ZucchiniStatus ToMojom(zucchini::status::Code input) {
    switch (input) {
      case kStatusSuccess:
        return patch::mojom::ZucchiniStatus::kStatusSuccess;
      case kStatusInvalidParam:
        return patch::mojom::ZucchiniStatus::kStatusInvalidParam;
      case kStatusFileReadError:
        return patch::mojom::ZucchiniStatus::kStatusFileReadError;
      case kStatusFileWriteError:
        return patch::mojom::ZucchiniStatus::kStatusFileWriteError;
      case kStatusPatchReadError:
        return patch::mojom::ZucchiniStatus::kStatusPatchReadError;
      case kStatusPatchWriteError:
        return patch::mojom::ZucchiniStatus::kStatusPatchWriteError;
      case kStatusInvalidOldImage:
        return patch::mojom::ZucchiniStatus::kStatusInvalidOldImage;
      case kStatusInvalidNewImage:
        return patch::mojom::ZucchiniStatus::kStatusInvalidNewImage;
      case kStatusDiskFull:
        return patch::mojom::ZucchiniStatus::kStatusDiskFull;
      case kStatusIoError:
        return patch::mojom::ZucchiniStatus::kStatusIoError;
      case kStatusFatal:
        return patch::mojom::ZucchiniStatus::kStatusFatal;
    }
    NOTREACHED();
  }

  static bool FromMojom(patch::mojom::ZucchiniStatus input,
                        zucchini::status::Code* out) {
    switch (input) {
      case patch::mojom::ZucchiniStatus::kStatusSuccess:
        *out = kStatusSuccess;
        return true;
      case patch::mojom::ZucchiniStatus::kStatusInvalidParam:
        *out = kStatusInvalidParam;
        return true;
      case patch::mojom::ZucchiniStatus::kStatusFileReadError:
        *out = kStatusFileReadError;
        return true;
      case patch::mojom::ZucchiniStatus::kStatusFileWriteError:
        *out = kStatusFileWriteError;
        return true;
      case patch::mojom::ZucchiniStatus::kStatusPatchReadError:
        *out = kStatusPatchReadError;
        return true;
      case patch::mojom::ZucchiniStatus::kStatusPatchWriteError:
        *out = kStatusPatchWriteError;
        return true;
      case patch::mojom::ZucchiniStatus::kStatusInvalidOldImage:
        *out = kStatusInvalidOldImage;
        return true;
      case patch::mojom::ZucchiniStatus::kStatusInvalidNewImage:
        *out = kStatusInvalidNewImage;
        return true;
      case patch::mojom::ZucchiniStatus::kStatusDiskFull:
        *out = kStatusDiskFull;
        return true;
      case patch::mojom::ZucchiniStatus::kStatusIoError:
        *out = kStatusIoError;
        return true;
      case patch::mojom::ZucchiniStatus::kStatusFatal:
        *out = kStatusFatal;
        return true;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // COMPONENTS_SERVICES_PATCH_PUBLIC_MOJOM_FILE_PATCHER_MOJOM_TRAITS_H_
