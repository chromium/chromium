// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_INTERNAL_PLUGIN_DELEGATE_H_
#define COMPONENTS_PDF_RENDERER_PDF_INTERNAL_PLUGIN_DELEGATE_H_

namespace url {
class Origin;
}  // namespace url

namespace pdf {

// Interface for embedder-provided operations required to create an instance of
// the internal PDF plugin.
class PdfInternalPluginDelegate {
 public:
  PdfInternalPluginDelegate();
  virtual ~PdfInternalPluginDelegate();

  // Returns `true` if the origin is allowed to create the internal PDF plugin.
  // Note that this applies to the origin of the parent of the frame that
  // contains the in-process plugin.
  virtual bool IsAllowedOrigin(const url::Origin& origin) const;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_INTERNAL_PLUGIN_DELEGATE_H_
