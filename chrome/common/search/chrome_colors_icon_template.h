// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_CHROME_COLORS_ICON_TEMPLATE_H_
#define CHROME_COMMON_SEARCH_CHROME_COLORS_ICON_TEMPLATE_H_

// Template for the icon svg.
// $1 - primary color
// $2 - secondary color
const char kChromeColorsIconTemplate[] =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" "
    "xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"64\" "
    "height=\"64\"><defs><path d=\"M32 64C14.34 64 0 49.66 0 32S14.34 0 32 "
    "0s32 14.34 32 32-14.34 32-32 32z\" id=\"a\"/><linearGradient id=\"b\" "
    "gradientUnits=\"userSpaceOnUse\" x1=\"32\" y1=\"32\" x2=\"32.08\" "
    "y2=\"32\"><stop offset=\"0\%\" stop-color=\"$2\"/><stop offset=\"100\%\" "
    "stop-color=\"$1\"/></linearGradient><clipPath id=\"c\"><use "
    "xlink:href=\"#a\"/></clipPath></defs><use xlink:href=\"#a\" "
    "fill=\"url(#b)\"/><g clip-path=\"url(#c)\"><use xlink:href=\"#a\" "
    "fill-opacity=\"0\" stroke=\"$1\" stroke-width=\"2\"/></g></svg>";

#endif  // CHROME_COMMON_SEARCH_CHROME_COLORS_ICON_TEMPLATE_H_
