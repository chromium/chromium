// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_IMAGE_DECODER_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_IMAGE_DECODER_H_

#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

class Profile;

class SharesheetImageDecoder {
 public:
  using DecodeCallback = base::OnceCallback<void(gfx::ImageSkia)>;

  SharesheetImageDecoder();
  ~SharesheetImageDecoder();
  SharesheetImageDecoder(const SharesheetImageDecoder&) = delete;
  SharesheetImageDecoder& operator=(const SharesheetImageDecoder&) = delete;

  void DecodeImage(apps::mojom::IntentPtr intent,
                   Profile* profile,
                   DecodeCallback callback);

 private:
  // Encodes the FilePath |url| into encoded bytes.
  void URLToEncodedBytes(const base::FilePath& url);

  // Decodes the string of encoded bytes |image_data|
  // into an SkBitmap in a sandboxed process.
  void DecodeURLForPreview(std::string image_data);

  // Converts the |decoded_image| into an ImageSkia.
  void BitMapToImage(const SkBitmap& decoded_image);

  DecodeCallback callback_;
  base::WeakPtrFactory<SharesheetImageDecoder> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_IMAGE_DECODER_H_
