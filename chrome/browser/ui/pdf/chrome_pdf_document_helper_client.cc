// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/chrome_pdf_document_helper_client.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/content_restriction.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/pdf/browser/pdf_frame_util.h"
#include "components/tabs/public/tab_interface.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "pdf/pdf_features.h"

namespace {

content::WebContents* GetWebContentsToUse(
    content::RenderFrameHost* render_frame_host) {
  // If we're viewing the PDF in a MimeHandlerViewGuest, use its embedder
  // WebContents.
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromRenderFrameHost(render_frame_host);
  return guest_view
             ? guest_view->embedder_web_contents()
             : content::WebContents::FromRenderFrameHost(render_frame_host);
}

bool MaybeShowFeaturePromo(const base::Feature& feature,
                           content::WebContents* contents) {
  auto* user_education_interface =
      BrowserUserEducationInterface::MaybeGetForWebContentsInTab(contents);
  if (!user_education_interface) {
    return false;
  }
  user_education_interface->MaybeShowFeaturePromo(
      user_education::FeaturePromoParams(feature));
  return true;
}

void MaybeHideSearchifyFeaturePromo(tabs::TabInterface* tab_interface) {
  auto* user_education_interface = BrowserUserEducationInterface::From(
      tab_interface->GetBrowserWindowInterface());
  if (user_education_interface) {
    user_education_interface->AbortFeaturePromo(
        feature_engagement::kIPHPdfSearchifyFeature);
  }
}

void LogGlicSummarizeMetrics(content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents_to_use =
      GetWebContentsToUse(render_frame_host);
  if (!web_contents_to_use) {
    return;
  }

  bool glic_enabled = glic::GlicEnabling::IsEnabledForProfile(
      Profile::FromBrowserContext(web_contents_to_use->GetBrowserContext()));
  base::UmaHistogramBoolean("PDF.GlicEnabled", glic_enabled);
  bool glic_summarize_button_enabled =
      pdf_extension_util::ShouldShowGlicSummarizeButton(
          web_contents_to_use->GetBrowserContext());
  base::UmaHistogramBoolean("PDF.GlicSummarizeButtonEnabled",
                            glic_summarize_button_enabled);
}

}  // namespace

ChromePDFDocumentHelperClient::ChromePDFDocumentHelperClient() = default;

ChromePDFDocumentHelperClient::~ChromePDFDocumentHelperClient() = default;

void ChromePDFDocumentHelperClient::OnDocumentLoadComplete(
    content::RenderFrameHost* render_frame_host) {
  MaybeShowFeaturePromo(feature_engagement::kIPHPdfInkSignaturesFeature,
                        GetWebContentsToUse(render_frame_host));

  auto* parent = render_frame_host->GetParent();
  bool is_pdf_viewer =
      parent && parent->GetLastCommittedURL().GetWithEmptyPath() ==
                    base::FilePath(ChromeContentClient::kPDFExtensionPluginPath)
                        .MaybeAsASCII();

  if (is_pdf_viewer) {
    LogGlicSummarizeMetrics(render_frame_host);
  }
}

void ChromePDFDocumentHelperClient::UpdateContentRestrictions(
    content::RenderFrameHost* render_frame_host,
    int content_restrictions) {
  // Speculative short-term-fix while we get at the root of
  // https://crbug.com/752822 .
  content::WebContents* web_contents_to_use =
      GetWebContentsToUse(render_frame_host);
  if (!web_contents_to_use) {
    return;
  }

  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(web_contents_to_use);
  // |core_tab_helper| is null for WebViewGuest.
  if (core_tab_helper) {
    core_tab_helper->UpdateContentRestrictions(content_restrictions);
  }
}

void ChromePDFDocumentHelperClient::OnSaveURL() {
  RecordDownloadSource(DOWNLOAD_INITIATED_BY_PDF_SAVE);
}

void ChromePDFDocumentHelperClient::SetPluginCanSave(
    content::RenderFrameHost* render_frame_host,
    bool can_save) {
  if (chrome_pdf::features::IsOopifPdfEnabled()) {
    auto* pdf_viewer_stream_manager =
        pdf::PdfViewerStreamManager::FromWebContents(
            content::WebContents::FromRenderFrameHost(render_frame_host));
    if (!pdf_viewer_stream_manager) {
      return;
    }

    content::RenderFrameHost* embedder_host =
        pdf_frame_util::GetEmbedderHost(render_frame_host);
    CHECK(embedder_host);

    pdf_viewer_stream_manager->SetPluginCanSave(embedder_host, can_save);
    return;
  }

  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromRenderFrameHost(render_frame_host);
  if (guest_view) {
    guest_view->SetPluginCanSave(can_save);
  }
}

void ChromePDFDocumentHelperClient::OnSearchifyStarted(
    content::RenderFrameHost* render_frame_host) {
  // Show the promo only when ScreenAI component is available and OCR can be
  // done.
  if (!screen_ai::ScreenAIInstallState::GetInstance()->IsComponentAvailable()) {
    return;
  }
  content::WebContents* web_contents = GetWebContentsToUse(render_frame_host);
  if (!MaybeShowFeaturePromo(feature_engagement::kIPHPdfSearchifyFeature,
                             web_contents)) {
    return;
  }
  auto* const tab = tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab) {
    return;
  }
  tab_subscriptions_.push_back(tab->RegisterWillDeactivate(
      base::BindRepeating(&MaybeHideSearchifyFeaturePromo)));
}
