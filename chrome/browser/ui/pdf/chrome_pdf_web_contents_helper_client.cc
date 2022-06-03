// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/chrome_pdf_web_contents_helper_client.h"

#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/common/content_restriction.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "ppapi/c/private/ppb_pdf.h"

namespace {

// For the UpdateContentRestrictions() call below, ensure the enum values in
// chrome/common/content_restriction.h and ppapi/c/private/ppb_pdf.h match.
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)

STATIC_ASSERT_ENUM(CONTENT_RESTRICTION_COPY, PP_CONTENT_RESTRICTION_COPY);
STATIC_ASSERT_ENUM(CONTENT_RESTRICTION_CUT, PP_CONTENT_RESTRICTION_CUT);
STATIC_ASSERT_ENUM(CONTENT_RESTRICTION_PASTE, PP_CONTENT_RESTRICTION_PASTE);
STATIC_ASSERT_ENUM(CONTENT_RESTRICTION_PRINT, PP_CONTENT_RESTRICTION_PRINT);
STATIC_ASSERT_ENUM(CONTENT_RESTRICTION_SAVE, PP_CONTENT_RESTRICTION_SAVE);

content::WebContents* GetWebContentsToUse(
    content::WebContents* web_contents) {
  // If we're viewing the PDF in a MimeHandlerViewGuest, use its embedder
  // WebContents.
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(web_contents);
  return guest_view ? guest_view->embedder_web_contents() : web_contents;
}

}  // namespace

ChromePDFWebContentsHelperClient::ChromePDFWebContentsHelperClient() = default;

ChromePDFWebContentsHelperClient::~ChromePDFWebContentsHelperClient() = default;

void ChromePDFWebContentsHelperClient::UpdateContentRestrictions(
    content::WebContents* contents,
    int content_restrictions) {
  // Speculative short-term-fix while we get at the root of
  // https://crbug.com/752822 .
  content::WebContents* web_contents_to_use = GetWebContentsToUse(contents);
  if (!web_contents_to_use)
    return;

  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(web_contents_to_use);
  // |core_tab_helper| is null for WebViewGuest.
  if (core_tab_helper)
    core_tab_helper->UpdateContentRestrictions(content_restrictions);
}

void ChromePDFWebContentsHelperClient::OnPDFHasUnsupportedFeature(
    content::WebContents* contents) {
  // There is no more Adobe plugin for PDF so there is not much we can do in
  // this case. Maybe simply download the file.
}

void ChromePDFWebContentsHelperClient::OnSaveURL(
    content::WebContents* contents) {
  RecordDownloadSource(DOWNLOAD_INITIATED_BY_PDF_SAVE);
}

void ChromePDFWebContentsHelperClient::SetPluginCanSave(
    content::WebContents* contents,
    bool can_save) {
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(contents);
  if (guest_view)
    guest_view->SetPluginCanSave(can_save);
}
