// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_COMMON_INTERNAL_PLUGIN_HELPERS_H_
#define COMPONENTS_PDF_COMMON_INTERNAL_PLUGIN_HELPERS_H_

namespace pdf {

// MIME type of the internal PDF plugin.
extern const char kInternalPluginMimeType[];

// Returns `true` if the internal PDF plugin may be used as an "externally
// handled" plugin instance. Such plugin instances load the source URL in a
// subframe, rather than creating a `blink::WebPlugin` object.
//
// Note that in the case of the internal PDF plugin, a second instance within
// the subframe eventually does get loaded as a normal `blink::WebPlugin`.
bool IsInternalPluginExternallyHandled();

}  // namespace pdf

#endif  // COMPONENTS_PDF_COMMON_INTERNAL_PLUGIN_HELPERS_H_
