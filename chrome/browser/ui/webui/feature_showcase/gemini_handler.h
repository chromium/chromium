// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_GEMINI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_GEMINI_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/feature_showcase/gemini.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace glic {
class GlicKeyedService;
}

class GeminiHandler : public feature_showcase::mojom::GeminiPageHandler {
 public:
  explicit GeminiHandler(
      mojo::PendingReceiver<feature_showcase::mojom::GeminiPageHandler>
          receiver,
      glic::GlicKeyedService* glic_service);
  GeminiHandler(const GeminiHandler&) = delete;
  GeminiHandler& operator=(const GeminiHandler&) = delete;
  ~GeminiHandler() override;

  // feature_showcase::mojom::GeminiPageHandler:
  void AcceptConsent() override;
  void SkipConsent() override;

 private:
  mojo::Receiver<feature_showcase::mojom::GeminiPageHandler> receiver_;
  const base::raw_ref<glic::GlicKeyedService> glic_service_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEATURE_SHOWCASE_GEMINI_HANDLER_H_
