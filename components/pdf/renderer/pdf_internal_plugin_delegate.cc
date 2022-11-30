// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_internal_plugin_delegate.h"

namespace pdf {

PdfInternalPluginDelegate::PdfInternalPluginDelegate() = default;
PdfInternalPluginDelegate::~PdfInternalPluginDelegate() = default;

bool PdfInternalPluginDelegate::IsAllowedOrigin(
    const url::Origin& origin) const {
  return false;
}

}  // namespace pdf
