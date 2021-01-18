// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_image_decode.h"

SharesheetImageDecode::SharesheetImageDecode() = default;

void SharesheetImageDecode::URLToEncodedBytes(const base::FilePath& url) {
  // TODO(crbug.com/2631548): Add code to encode the url by adding
  // a new task to the main thread, use FileToString() function and
  // pass the result into DecodeURLForPreview.
  NOTIMPLEMENTED();
}

void SharesheetImageDecode::DecodeURLForPreview(std::string image_data) {
  // TODO(crbug.com/2631548): Run sandboxed process using
  // DecodeImageIsolated and pass result into BitMapToImage.
  NOTIMPLEMENTED();
}

void SharesheetImageDecode::BitMapToImage(const SkBitmap& decoded_image) {
  // TODO(crbug.com/2631548): Use CreateFrom1xBitmap function to convert
  // encoded bitmap into type imageskia and add to content previews view.
  NOTIMPLEMENTED();
}
