// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/printing/chrome_print_render_frame_helper_delegate.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"
#include "extensions/renderer/guest_view/mime_handler_view/post_message_support.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

ChromePrintRenderFrameHelperDelegate::ChromePrintRenderFrameHelperDelegate() =
    default;

ChromePrintRenderFrameHelperDelegate::~ChromePrintRenderFrameHelperDelegate() =
    default;

// Return the PDF object element if |frame| is the out of process PDF extension.
blink::WebElement ChromePrintRenderFrameHelperDelegate::GetPdfElement(
    blink::WebLocalFrame* frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  GURL url = frame->GetDocument().Url();
  bool inside_print_preview = url.GetOrigin() == chrome::kChromeUIPrintURL;
  bool inside_pdf_extension =
      url.SchemeIs(extensions::kExtensionScheme) &&
      url.host_piece() == extension_misc::kPdfExtensionId;
  if (inside_print_preview || inside_pdf_extension) {
    // <object> with id="plugin" is created in
    // chrome/browser/resources/pdf/pdf_viewer_base.js.
    auto viewer_element = frame->GetDocument().GetElementById("viewer");
    if (!viewer_element.IsNull() && !viewer_element.ShadowRoot().IsNull()) {
      auto plugin_element =
          viewer_element.ShadowRoot().QuerySelector("#plugin");
      if (!plugin_element.IsNull()) {
        return plugin_element;
      }
    }
    NOTREACHED();
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return blink::WebElement();
}

bool ChromePrintRenderFrameHelperDelegate::IsPrintPreviewEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(switches::kDisablePrintPreview);
}

bool ChromePrintRenderFrameHelperDelegate::OverridePrint(
    blink::WebLocalFrame* frame) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* post_message_support =
      extensions::PostMessageSupport::FromWebLocalFrame(frame);
  if (post_message_support) {
    // This message is handled in chrome/browser/resources/pdf/pdf_viewer.js and
    // instructs the PDF plugin to print. This is to make window.print() on a
    // PDF plugin document correctly print the PDF. See
    // https://crbug.com/448720.
    base::DictionaryValue message;
    message.SetString("type", "print");
    post_message_support->PostMessageFromValue(message);
    return true;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return false;
}

bool ChromePrintRenderFrameHelperDelegate::ShouldGenerateTaggedPDF() {
  return true;
}
