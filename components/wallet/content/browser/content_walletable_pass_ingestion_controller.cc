// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/content/browser/content_walletable_pass_ingestion_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace wallet {

ContentWalletablePassIngestionController::
    ContentWalletablePassIngestionController(content::WebContents* web_contents,
                                             WalletablePassClient* client)
    : WalletablePassIngestionController(client),
      content::WebContentsObserver(web_contents) {}

ContentWalletablePassIngestionController::
    ~ContentWalletablePassIngestionController() = default;

void ContentWalletablePassIngestionController::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (render_frame_host->IsInPrimaryMainFrame()) {
    StartWalletablePassDetectionFlow(validated_url);
  }
}

std::string ContentWalletablePassIngestionController::GetPageTitle() const {
  return base::UTF16ToUTF8(web_contents()->GetTitle());
}

void ContentWalletablePassIngestionController::GetAnnotatedPageContent(
    AnnotatedPageContentCallback callback) {
  blink::mojom::AIPageContentOptionsPtr ai_page_content_options =
      optimization_guide::DefaultAIPageContentOptions(
          /*on_critical_path =*/true);
  optimization_guide::GetAIPageContent(
      web_contents(), std::move(ai_page_content_options),
      base::BindOnce(
          [](AnnotatedPageContentCallback callback,
             optimization_guide::AIPageContentResultOrError result) {
            if (!result.has_value()) {
              std::move(callback).Run(std::nullopt);
              return;
            }
            std::move(callback).Run(std::move(result->proto));
          },
          std::move(callback)));
}

}  // namespace wallet
