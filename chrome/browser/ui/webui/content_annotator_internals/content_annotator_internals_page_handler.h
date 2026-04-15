// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONTENT_ANNOTATOR_INTERNALS_CONTENT_ANNOTATOR_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CONTENT_ANNOTATOR_INTERNALS_CONTENT_ANNOTATOR_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "components/accessibility_annotator/core/logging/accessibility_annotator_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content_annotator_internals {

class ContentAnnotatorInternalsPageHandler
    : public accessibility_annotator_internals::mojom::PageHandler {
 public:
  explicit ContentAnnotatorInternalsPageHandler(
      mojo::PendingReceiver<
          accessibility_annotator_internals::mojom::PageHandler> receiver,
      mojo::PendingRemote<accessibility_annotator_internals::mojom::Page> page,
      Profile* profile);
  ContentAnnotatorInternalsPageHandler(
      const ContentAnnotatorInternalsPageHandler&) = delete;
  ContentAnnotatorInternalsPageHandler& operator=(
      const ContentAnnotatorInternalsPageHandler&) = delete;
  ~ContentAnnotatorInternalsPageHandler() override;

  // accessibility_annotator_internals::mojom::PageHandler:
  void GetAnnotatedContent(GetAnnotatedContentCallback callback) override;
  void ClearAnnotatedContent(
      ClearAnnotatedContentCallback callback) override;
  void DeleteAnnotatedContent(const std::vector<int64_t>& visit_ids,
                              DeleteAnnotatedContentCallback callback) override;

 private:
  mojo::Receiver<accessibility_annotator_internals::mojom::PageHandler>
      receiver_;
  mojo::Remote<accessibility_annotator_internals::mojom::Page> page_;

  // The WebUI interacts with the AccessibilityAnnotatorBackend, which is a
  // KeyedService tied to the lifetime of the Profile. As a result, the Profile
  // will always outlive this page handler.
  raw_ptr<Profile> profile_ = nullptr;
};

}  // namespace content_annotator_internals

#endif  // CHROME_BROWSER_UI_WEBUI_CONTENT_ANNOTATOR_INTERNALS_CONTENT_ANNOTATOR_INTERNALS_PAGE_HANDLER_H_
