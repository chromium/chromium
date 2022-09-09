// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_CHROME_MANIFEST_HANDLERS_H_
#define CHROME_COMMON_EXTENSIONS_CHROME_MANIFEST_HANDLERS_H_

namespace extensions {

// Registers all manifest handlers used in Chrome. Should be called
// once in each process. See also extensions/common/common_manifest_handlers.h.
void RegisterChromeManifestHandlers();

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_CHROME_MANIFEST_HANDLERS_H_
