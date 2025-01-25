// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pdf/chrome_pdf_internal_plugin_delegate.h"

#include "base/containers/span.h"
#include "chrome/common/webui_url_constants.h"
#include "components/pdf/common/pdf_util.h"
#include "url/gurl.h"
#include "url/origin.h"

ChromePdfInternalPluginDelegate::ChromePdfInternalPluginDelegate() = default;

ChromePdfInternalPluginDelegate::~ChromePdfInternalPluginDelegate() = default;

bool ChromePdfInternalPluginDelegate::IsAllowedOrigin(
    const url::Origin& origin) const {
  // TODO(crbug.com/392037570): Remove this duplicate logic by removing this
  // file altogether.
  auto allowed_origin = url::Origin::Create(GURL(chrome::kChromeUIPrintURL));
  return IsPdfInternalPluginAllowedOrigin(origin,
                                          base::span_from_ref(allowed_origin));
}
