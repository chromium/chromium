// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_BASE_FAVICON_URL_PARSER_H_
#define COMPONENTS_FAVICON_BASE_FAVICON_URL_PARSER_H_

#include <stddef.h>

#include <string>

#include "ui/gfx/favicon_size.h"

namespace chrome {

struct ParsedFaviconPath {
  ParsedFaviconPath();
  ParsedFaviconPath(const ParsedFaviconPath& other);
  ParsedFaviconPath& operator=(const ParsedFaviconPath& other);

  // URL pointing to the page whose favicon we want.
  std::string page_url;

  // URL pointing directly to favicon image. If both |page_url| and |icon_url|
  // are specified, |page_url| will have precedence. At least one between
  // |page_url| and |icon_url| must be non-empty.
  std::string icon_url;

  // The size of the requested favicon in dip.
  int size_in_dip = gfx::kFaviconSize;

  // The device scale factor of the requested favicon.
  float device_scale_factor = 1.0f;

  // The index of the first character (relative to the path) where the the URL
  // from which the favicon is being requested is located.
  size_t path_index = std::string::npos;

  // Whether we should allow making a request to the favicon server as fallback.
  bool allow_favicon_server_fallback = false;

  // Whether we should show a fallback monogram in place of the default favicon.
  bool show_fallback_monogram = false;

  // Whether we should ignore the theme when themeing the default favicon and
  // just return the light mode version.
  bool force_light_mode = false;
};

// Enum describing the two possible url formats: the legacy chrome://favicon
// and chrome://favicon2.
// - chrome://favicon format:
//   chrome://favicon/size&scaleFactor/iconUrl/url
// Some parameters are optional as described below. However, the order of the
// parameters is not interchangeable.
//
// Parameter:
//  'url'               Required
//    Specifies the page URL of the requested favicon. If the 'iconurl'
//    parameter is specified, the URL refers to the URL of the favicon image
//    instead.
//  'size&scaleFactor'  Optional
//    Values: ['size/aa@bx/']
//      Specifies the requested favicon's size in DIP (aa) and the requested
//      favicon's scale factor. (b).
//      The supported requested DIP sizes are: 16x16, 32x32 and 64x64.
//      If the parameter is unspecified, the requested favicon's size defaults
//      to 16 and the requested scale factor defaults to 1x.
//      Example: chrome://favicon/size/16@2x/https://www.google.com/
//  'iconUrl'           Optional
//    Values: ['iconUrl']
//    'iconurl': Specifies that the url parameter refers to the URL of
//    the favicon image as opposed to the URL of the page that the favicon is
//    on.
//    Example: chrome://favicon/iconurl/https://www.google.com/favicon.ico
//
// - chrome://favicon2 format:
//   chrome://favicon2/?queryParameters
// Standard URL query parameters are used as described below.
//
// URL Parameters:
//  'pageUrl'
//    URL pointing to the page whose favicon we want.
//  'iconUrl'
//    URL pointing directly to favicon image associated with `pageUrl`.
//    Pointed image will not necessarily have the most appropriate resolution
//    to the user's device.
//
// At least one of the two must be provided and non-empty. If both `pageUrl`
// and `iconUrl` are passed, `pageUrl` will have precedence.
//
// Other parameters:
//  'size'  Optional
//      Specifies the requested favicon's size in DIP. If unspecified, defaults
//      to 16.
//    Example: chrome://favicon2/?size=32
//  'scaleFactor'  Optional
//      Values: ['SCALEx']
//      Specifies the requested favicon's scale factor. If unspecified, defaults
//      to 1x.
//    Example: chrome://favicon2/?scaleFactor=1.2x
//
//  'allowGoogleServerFallback' Optional
//      Values: ['1', '0']
//      Specifies whether we are allowed to fall back to an external server
//      request (by page url) in case the icon is not found locally.
//      Setting this to 1 while not providing a non-empty page url will cause
//      parsing to fail.
enum class FaviconUrlFormat {
  // Legacy chrome://favicon format.
  kFaviconLegacy,
  // chrome://favicon2 format.
  kFavicon2,
};

// Parses |path| according to |format|, returning true if successful. The result
// of the parsing will be stored in the struct pointed by |parsed|.
bool ParseFaviconPath(const std::string& path,
                      FaviconUrlFormat format,
                      ParsedFaviconPath* parsed);

}  // namespace chrome

#endif  // COMPONENTS_FAVICON_BASE_FAVICON_URL_PARSER_H_
