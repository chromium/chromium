// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_IOS_PARSING_UTILS_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_IOS_PARSING_UTILS_H_

#import <CoreGraphics/CoreGraphics.h>

#import <optional>
#import <string>

#import "base/values.h"
#import "url/gurl.h"

namespace web {
class WebState;
}  // namespace web

namespace shared_highlighting {

// Returns whether |value| is a dictionary value, and is not empty.
BOOL IsValidDictValue(const base::Value* value);

// Attempts to parse the given |dict| into a CGRect. If |dict| does not map
// into the expected structure, an empty std::optional instance will be
// returned.
std::optional<CGRect> ParseRect(const base::Value::Dict* dict);

// Attempts to parse the given |url_value| into a GURL instance. If |url_value|
// is empty or invalid, an empty std::optional instance will be returned.
std::optional<GURL> ParseURL(const std::string* url_value);

// Converts a given |web_view_rect| into its browser coordinates counterpart.
// Uses the given |web_state| to do the conversion.
CGRect ConvertToBrowserRect(CGRect web_view_rect, web::WebState* web_state);

}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_IOS_PARSING_UTILS_H_
