// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/mojom/typemaps/windows_handle_mojom_traits.h"

#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

using chrome_cleaner::mojom::PredefinedHandle;
using chrome_cleaner::mojom::WindowsHandleDataView;

namespace {

bool ToPredefinedHandle(HANDLE handle,
                        PredefinedHandle* out_predefined_handle) {
  DCHECK(out_predefined_handle);

  if (handle == nullptr) {
    *out_predefined_handle = PredefinedHandle::NULL_HANDLE;
    return true;
  }
  if (handle == INVALID_HANDLE_VALUE) {
    *out_predefined_handle = PredefinedHandle::INVALID_HANDLE;
    return true;
  }
  if (handle == HKEY_CLASSES_ROOT) {
    *out_predefined_handle = PredefinedHandle::CLASSES_ROOT;
    return true;
  }
  if (handle == HKEY_CURRENT_CONFIG) {
    *out_predefined_handle = PredefinedHandle::CURRENT_CONFIG;
    return true;
  }
  if (handle == HKEY_CURRENT_USER) {
    *out_predefined_handle = PredefinedHandle::CURRENT_USER;
    return true;
  }
  if (handle == HKEY_LOCAL_MACHINE) {
    *out_predefined_handle = PredefinedHandle::LOCAL_MACHINE;
    return true;
  }
  if (handle == HKEY_USERS) {
    *out_predefined_handle = PredefinedHandle::USERS;
    return true;
  }
  return false;
}

bool IsPredefinedHandle(HANDLE handle) {
  PredefinedHandle unused;
  return ToPredefinedHandle(handle, &unused);
}

bool FromPredefinedHandle(PredefinedHandle predefined_handle,
                          HANDLE* out_handle) {
  DCHECK(out_handle);

  switch (predefined_handle) {
    case PredefinedHandle::NULL_HANDLE:
      *out_handle = nullptr;
      return true;

    case PredefinedHandle::INVALID_HANDLE:
      *out_handle = INVALID_HANDLE_VALUE;
      return true;

    case PredefinedHandle::CLASSES_ROOT:
      *out_handle = HKEY_CLASSES_ROOT;
      return true;

    case PredefinedHandle::CURRENT_CONFIG:
      *out_handle = HKEY_CURRENT_CONFIG;
      return true;

    case PredefinedHandle::CURRENT_USER:
      *out_handle = HKEY_CURRENT_USER;
      return true;

    case PredefinedHandle::LOCAL_MACHINE:
      *out_handle = HKEY_LOCAL_MACHINE;
      return true;

    case PredefinedHandle::USERS:
      *out_handle = HKEY_USERS;
      return true;

    default:
      return false;
  }
}

// Duplicates a handle in the current process. Returns INVALID_HANDLE_VALUE on
// error.
HANDLE DuplicateWindowsHandle(HANDLE source_handle) {
  const HANDLE current_process = ::GetCurrentProcess();
  HANDLE new_handle = INVALID_HANDLE_VALUE;
  if (::DuplicateHandle(current_process, source_handle, current_process,
                        &new_handle, 0, FALSE, DUPLICATE_SAME_ACCESS) == 0) {
    PLOG(ERROR) << "Error duplicating handle " << source_handle;
    return INVALID_HANDLE_VALUE;
  }
  return new_handle;
}

}  // namespace

// static
PredefinedHandle EnumTraits<PredefinedHandle, HANDLE>::ToMojom(HANDLE handle) {
  PredefinedHandle result;
  CHECK(ToPredefinedHandle(handle, &result));
  return result;
}

// static
bool EnumTraits<PredefinedHandle, HANDLE>::FromMojom(PredefinedHandle input,
                                                     HANDLE* output) {
  return FromPredefinedHandle(input, output);
}

// static
mojo::ScopedHandle UnionTraits<WindowsHandleDataView, HANDLE>::raw_handle(
    HANDLE handle) {
  DCHECK_EQ(WindowsHandleDataView::Tag::RAW_HANDLE, GetTag(handle));

  if (IsPredefinedHandle(handle)) {
    CHECK(false) << "Accessor raw_handle() should only be called when the "
                    "union's tag is RAW_HANDLE.";
    return mojo::ScopedHandle();
  }

  HANDLE duplicate_handle = DuplicateWindowsHandle(handle);
  return WrapPlatformFile(duplicate_handle);
}

// static
PredefinedHandle UnionTraits<WindowsHandleDataView, HANDLE>::special_handle(
    HANDLE handle) {
  DCHECK_EQ(WindowsHandleDataView::Tag::SPECIAL_HANDLE, GetTag(handle));

  PredefinedHandle predefined_handle;
  if (ToPredefinedHandle(handle, &predefined_handle))
    return predefined_handle;

  CHECK(false) << "Accessor special_handle() should only be called when the "
                  "union's tag is SPECIAL_HANDLE.";
  return PredefinedHandle::INVALID_HANDLE;
}

// static
WindowsHandleDataView::Tag UnionTraits<WindowsHandleDataView, HANDLE>::GetTag(
    HANDLE handle) {
  return IsPredefinedHandle(handle) ? WindowsHandleDataView::Tag::SPECIAL_HANDLE
                                    : WindowsHandleDataView::Tag::RAW_HANDLE;
}

// static
bool UnionTraits<WindowsHandleDataView, HANDLE>::Read(
    WindowsHandleDataView windows_handle_view,
    HANDLE* out) {
  if (windows_handle_view.is_raw_handle()) {
    HANDLE handle;
    MojoResult mojo_result =
        UnwrapPlatformFile(windows_handle_view.TakeRawHandle(), &handle);
    if (mojo_result != MOJO_RESULT_OK) {
      *out = INVALID_HANDLE_VALUE;
      return false;
    }
    *out = handle;
    return true;
  }

  return windows_handle_view.ReadSpecialHandle(out);
}

}  // namespace mojo
