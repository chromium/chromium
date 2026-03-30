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

  static zucchini::status::Code FromMojom(patch::mojom::ZucchiniStatus input) {
    switch (input) {
      case patch::mojom::ZucchiniStatus::kStatusSuccess:
        return kStatusSuccess;
      case patch::mojom::ZucchiniStatus::kStatusInvalidParam:
        return kStatusInvalidParam;
      case patch::mojom::ZucchiniStatus::kStatusFileReadError:
        return kStatusFileReadError;
      case patch::mojom::ZucchiniStatus::kStatusFileWriteError:
        return kStatusFileWriteError;
      case patch::mojom::ZucchiniStatus::kStatusPatchReadError:
        return kStatusPatchReadError;
      case patch::mojom::ZucchiniStatus::kStatusPatchWriteError:
        return kStatusPatchWriteError;
      case patch::mojom::ZucchiniStatus::kStatusInvalidOldImage:
        return kStatusInvalidOldImage;
      case patch::mojom::ZucchiniStatus::kStatusInvalidNewImage:
        return kStatusInvalidNewImage;
      case patch::mojom::ZucchiniStatus::kStatusDiskFull:
        return kStatusDiskFull;
      case patch::mojom::ZucchiniStatus::kStatusIoError:
        return kStatusIoError;
      case patch::mojom::ZucchiniStatus::kStatusFatal:
        return kStatusFatal;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // COMPONENTS_SERVICES_PATCH_PUBLIC_MOJOM_FILE_PATCHER_MOJOM_TRAITS_H_
