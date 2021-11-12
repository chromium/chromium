// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_CAST_REDIRECT_MANIFEST_HANDLER_H_
#define CHROMECAST_COMMON_CAST_REDIRECT_MANIFEST_HANDLER_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/user_script.h"
#include "url/gurl.h"

namespace chromecast {

// Parses the "cast_redirect" and "cast_url" manifest keys.
class CastRedirectHandler : public extensions::ManifestHandler {
 public:
  CastRedirectHandler();

  CastRedirectHandler(const CastRedirectHandler&) = delete;
  CastRedirectHandler& operator=(const CastRedirectHandler&) = delete;

  ~CastRedirectHandler() override;

  bool Parse(extensions::Extension* extension, std::u16string* error) override;
  bool Validate(
      const extensions::Extension* extension,
      std::string* error,
      std::vector<extensions::InstallWarning>* warnings) const override;

  static bool ParseUrl(std::string* out_url,
                       const extensions::Extension* extension,
                       const GURL& url);

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace chromecast

#endif  // CHROMECAST_COMMON_CAST_REDIRECT_MANIFEST_HANDLER_H_
