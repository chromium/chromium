// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/util_read_icon.h"

#include <windows.h>

#include <string>

#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "base/win/scoped_gdi_object.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image_skia.h"

using chrome::mojom::IconSize;

namespace {

gfx::ImageSkia LoadIcon(const wchar_t* filename, int size, float scale) {
  base::ScopedNativeLibrary library(::LoadLibraryEx(
      filename, nullptr,
      LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));
  if (!library.is_valid())
    return gfx::ImageSkia();

  // Find the first icon referenced in the file.  This matches Explorer.
  LPWSTR id = nullptr;
  // Because the lambda below returns FALSE, EnumResourceNames() itself will
  // return FALSE even when it "succeeds", so ignore its return value.
  ::EnumResourceNames(
      library.get(), RT_GROUP_ICON,
      [](HMODULE, LPCWSTR, LPWSTR name, LONG_PTR param) {
        *reinterpret_cast<LPWSTR*>(param) =
            IS_INTRESOURCE(name) ? name : _wcsdup(name);
        return FALSE;
      },
      reinterpret_cast<LONG_PTR>(&id));

  base::win::ScopedHICON icon(static_cast<HICON>(
      ::LoadImage(library.get(), id, IMAGE_ICON, size, size, LR_DEFAULTCOLOR)));
  if (!IS_INTRESOURCE(id))
    free(id);
  if (!icon.is_valid())
    return gfx::ImageSkia();

  const SkBitmap bitmap = IconUtil::CreateSkBitmapFromHICON(icon.get());
  if (bitmap.isNull())
    return gfx::ImageSkia();

  gfx::ImageSkia image_skia(gfx::ImageSkiaRep(bitmap, scale));
  image_skia.MakeThreadSafe();
  return image_skia;
}

}  // namespace

UtilReadIcon::UtilReadIcon(
    mojo::PendingReceiver<chrome::mojom::UtilReadIcon> receiver)
    : receiver_(this, std::move(receiver)) {}

UtilReadIcon::~UtilReadIcon() = default;

// This is exposed to the browser process only. |filename| is a path to
// a downloaded file, such as an |.exe|.
void UtilReadIcon::ReadIcon(const base::FilePath& filename,
                            IconSize icon_size,
                            float scale,
                            ReadIconCallback callback) {
  int size = 0;
  // See IconLoader::IconSize.
  switch (icon_size) {
    case IconSize::kSmall:
      size = 16;
      break;
    case IconSize::kNormal:
      size = 32;
      break;
    case IconSize::kLarge:
      size = 48;
      break;
    default:
      NOTREACHED();
  }
  std::move(callback).Run(
      LoadIcon(filename.value().c_str(), size * scale, scale),
      filename.AsUTF16Unsafe());
}
