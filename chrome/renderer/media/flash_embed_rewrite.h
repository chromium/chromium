// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_FLASH_EMBED_REWRITE_H_
#define CHROME_RENDERER_MEDIA_FLASH_EMBED_REWRITE_H_

class GURL;

// Rewrites Flash embed URLs to their modern HTML5 equivalents.
// NOTE that this is not dead code and still has uses. See
// https://crrev.com/c/3199455 for discussion.
class FlashEmbedRewrite {
 public:
  // Entry point that will then call a private website-specific method.
  static GURL RewriteFlashEmbedURL(const GURL&);

 private:
  // YouTube specific method.
  static GURL RewriteYouTubeFlashEmbedURL(const GURL&);

  // Dailymotion specific method.
  static GURL RewriteDailymotionFlashEmbedURL(const GURL&);

  // Vimeo specific method.
  static GURL RewriteVimeoFlashEmbedURL(const GURL&);
};

#endif  // CHROME_RENDERER_MEDIA_FLASH_EMBED_REWRITE_H_
