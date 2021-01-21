// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_IMAGE_DECODE_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_IMAGE_DECODE_H_

#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class SharesheetImageDecode {
 public:
  SharesheetImageDecode();
  ~SharesheetImageDecode();
  SharesheetImageDecode(const SharesheetImageDecode&) = delete;
  SharesheetImageDecode& operator=(const SharesheetImageDecode&) = delete;

 private:
  // Encodes the FilePath |url| into encoded bytes.
  void URLToEncodedBytes(const base::FilePath& url);

  // Decodes the string of encoded bytes |image_data|
  // into an SkBitmap in a sandboxed process.
  void DecodeURLForPreview(std::string image_data);

  // Converts the |decoded_image| into an ImageSkia.
  void BitMapToImage(const SkBitmap& decoded_image);

  base::WeakPtrFactory<SharesheetImageDecode> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_IMAGE_DECODE_H_
