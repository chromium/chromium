// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_FLASH_EMBED_REWRITE_H_
#define CHROME_RENDERER_MEDIA_FLASH_EMBED_REWRITE_H_

class GURL;

class FlashEmbedRewrite {
 public:
  // Entry point that will then call a private website-specific method.
  static GURL RewriteFlashEmbedURL(const GURL&);

 private:
  // YouTube specific method.
  static GURL RewriteYouTubeFlashEmbedURL(const GURL&);

  // Dailymotion specific method.
  static GURL RewriteDailymotionFlashEmbedURL(const GURL&);
};

#endif  // CHROME_RENDERER_MEDIA_FLASH_EMBED_REWRITE_H_
