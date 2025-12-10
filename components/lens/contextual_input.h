// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_CONTEXTUAL_INPUT_H_
#define COMPONENTS_LENS_CONTEXTUAL_INPUT_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/raw_span.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/sessions/core/session_id.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace lens {
// Data struct representing context input data bytes.
// Moved from lens::PageContent
struct ContextualInput {
  ContextualInput();
  ContextualInput(std::vector<uint8_t> bytes, lens::MimeType content_type);
  ContextualInput(const ContextualInput& other);
  ~ContextualInput();

 public:
  std::vector<uint8_t> bytes_;
  lens::MimeType content_type_;
};

// Data struct for all the contextual information.
struct ContextualInputData {
  ContextualInputData();
  ~ContextualInputData();

  ContextualInputData(const ContextualInputData&);
  ContextualInputData& operator=(const ContextualInputData&) = default;

  // A wrapper for the ContextualInput this ContextualInputData contains.
  std::optional<std::vector<ContextualInput>> context_input;
  // The mime type of this content.
  std::optional<lens::MimeType> primary_content_type;
  // If the context is a webpage pr pdf, this is the URL associated with it.
  std::optional<GURL> page_url;
  // If the context is a webpage or pdf, this is the title of it.
  std::optional<std::string> page_title;
  // If the context is a pdf, this is the current viewed page.
  std::optional<uint32_t> pdf_current_page;
  // TODO(crbug.com/462520491): Set the tab handle from the contextual
  // searchbox handler.
  // If populated, the session id corresponding to the tab.
  std::optional<SessionID> tab_session_id;
  // If the context is a webpage or pdf, this is the viewport screenshot.
  std::optional<std::vector<uint8_t>> viewport_screenshot_bytes;
  // If the context is a webpage or pdf, and viewport_screenshot_bytes is null,
  // this is the viewport screenshot.
  std::optional<SkBitmap> viewport_screenshot;
  // Whether or not webpage or pdf context is eligible.
  std::optional<bool> is_page_context_eligible;
  // If set, the context id to use for referring to this context in the server.
  // Followup uploads for an existing document should re-use the same context
  // id.
  std::optional<uint64_t> context_id;
};

}  // namespace lens

#endif  // COMPONENTS_LENS_CONTEXTUAL_INPUT_H_
