// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pdf/chrome_pdf_internal_plugin_delegate.h"

#include "components/pdf/common/pdf_util.h"

ChromePdfInternalPluginDelegate::ChromePdfInternalPluginDelegate() = default;

ChromePdfInternalPluginDelegate::~ChromePdfInternalPluginDelegate() = default;

bool ChromePdfInternalPluginDelegate::IsAllowedOrigin(
    const url::Origin& origin) const {
  return IsPdfInternalPluginAllowedOrigin(origin);
}
